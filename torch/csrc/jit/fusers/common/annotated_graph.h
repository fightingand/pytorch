#pragma once

#include "torch/csrc/jit/fusers/fusion_interface.h"
#include "torch/csrc/jit/fusers/common/tensor_desc.h"

#include "torch/csrc/jit/ir.h"

namespace torch { namespace jit {

struct AnnotatedGraph {
  // short-term storage only, so it borrows Graph.
  AnnotatedGraph(Graph& graph, int device)
  : graph(&graph), device(device) {}
  
  Graph* graph = nullptr; // TODO: this should really be const
  int device = kCPUDevice;
  std::vector<TensorDesc> input_desc;
  std::vector<TensorDesc> output_desc;
};

} // namespace jit 
} // namespace torch
