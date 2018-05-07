#include "caffe2/opt/fuse_conv_relu.h"

namespace caffe2 { namespace opt {

void FuseConvRelu(
    nom::repr::NNModule* nn,
    std::function<bool(const repr::Conv& conv)> should_fuse,
    std::function<void(repr::Conv* conv)> postprocess) {
  for (auto node_pair : repr::nn::dataIterator<repr::Conv>(nn->dataFlow)) {
    repr::NNGraph::NodeRef conv_node;
    repr::Conv* conv;
    std::tie(conv, conv_node) = node_pair;

    // Check topological feasibility
    auto conv_outputs = repr::nn::getOutputs(conv_node);
    if (conv_outputs.size() != 1) {
      continue;
    }
    auto conv_output = conv_outputs.front();

    auto consumers = repr::nn::getConsumers(conv_output);
    if (consumers.size() != 1) {
      continue;
    }
    if (!repr::nn::is<repr::Relu>(consumers.front())) {
      continue;
    }
    auto relu_node = consumers.front();

    auto relu_outputs = repr::nn::getOutputs(relu_node);
    if (relu_outputs.size() != 1) {
      continue;
    }

    // Check feasibility with application specific logic
    if (!should_fuse(*conv)) {
     continue;
    }

    // Ready to fuse
    auto relu_output = relu_outputs.front();
    auto output_tensor = repr::nn::get<repr::Tensor>(relu_output);
    auto output_node = relu_output;
    auto input_tensor = repr::nn::get<repr::Tensor>(repr::nn::getInputs(conv_node).front());

    // Conv cannot be in-place
    if (output_tensor->getName() != input_tensor->getName()) {
      nn.dataFlow.replaceNode(conv_output, relu_output);
      nn.dataFlow.deleteNode(relu_node);
      nn.dataFlow.deleteNode(conv_output);
    } else {
      nn.dataFlow.replaceNode(relu_output, conv_output);
      output_tensor = repr::nn::get<repr::Tensor>(conv_output);
      output_node = conv_output;
      nn.dataFlow.deleteNode(relu_node);
      nn.dataFlow.deleteNode(relu_output);
    }

    // We may have accidentally made the next op in-place
    // In future iterations of transformations this won't be an issue,
    // but current caffe2 predictor usage requires things like
    // external_input and output to be unchanged.
    bool rectify_inplace = false;
    for (auto& consumer : repr::nn::getConsumers(output_node)) {
      for (auto& consumer_output : repr::nn::getOutputs(consumer)) {
        auto co_name = repr::nn::get<repr::Tensor>(consumer_output)->getName();
        if (co_name == output_tensor->getName()) {
          rectify_inplace = true;
        }
      }
    }
    if (rectify_inplace) {
      auto new_output = nn.dataFlow.createNode(make_unique<repr::Tensor>(output_tensor->getName() + "_fusion_fix"));
      nn.dataFlow.replaceNode(output_node, new_output);
    }

    // Application specific logic for postprocessing the conv node
    postprocess(conv)
  }
}

}}
