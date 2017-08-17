#include "torch/csrc/jit/graph_fuser.h"
#include <unordered_map>

namespace torch { namespace jit {

std::unordered_set<NodeKind> simple_mappable = {
  kSigmoid,
  kTanh,
  kMul,
  kAdd,
  kNegate
};

bool isSimpleMap(Node *node) {
  return simple_mappable.count(node->kind());
}

struct GraphFuser {
  std::unique_ptr<Graph>& graph;

  // Used to order nodes so we alway consider producer-consumer fusions
  // in reverse topological order.
  // If topological_index[a] > topological_index[b] then a occurs after b.
  // Because nodes can be added to this graph during optimization, this mapping is not bijective.
  // Newly generated nodes will copy the location where they are inserted.
  std::unordered_map<Node*,size_t> topological_index;

  GraphFuser(std::unique_ptr<Graph>& graph)
  : graph(graph) {}

  bool isFusable(Node * node) {
    return isSimpleMap(node) || node->kind() == kFusionGroup;
  }

  // necessary condition for fusion. If all of the uses of producer are consumer
  // then it is safe to merge producer into consumer, because it doesn't have any other uses
  // If there are other uses, but they occur _after_ consumer, then we can still merge in producer
  // with consumer, by rewriting those later uses to use the version of producer generated by the fused blob
  // In this case, producer becomes an output of the fusion group.
  bool allUsersAreThisConsumerOrOccurAfterIt(Node * consumer, Node * producer) {
    for(auto u : producer->uses()) {
      if(u.user != consumer && topological_index[consumer] > topological_index[u.user])
        return false;
    }
    return true;
  }
  bool allUsersAreThisConsumer(Node * consumer, Node * producer) {
    for(auto u : producer->uses()) {
      if(u.user != consumer)
        return false;
    }
    return true;
  }

  bool shouldFuse(Node * consumer, Node * producer) {
    // this handles cases where producer can be moved _into_ the fusion group of consumer.
    // TODO: extend to fusion of consumer into _producer's_ fusion blob
    // if the consumer allInputsAreThisProducer(consumer,producer)
    // we can move the consumer up into the producer.
    // but this requires better handling of merging fusion groups so it is not done now
    return isFusable(producer) && allUsersAreThisConsumerOrOccurAfterIt(consumer, producer);
  }

  // insert a producer node into a consuming fusion group.
  // DOES NOT WORK if n is a consumer of an output of the fusion group
  // returns the node _inside_ the group that represents the node
  Node * mergeNodeIntoGroup(FusionGroup * group, Node * n) {
    auto & subgraph = group->subgraph();
    auto & inputs = group->inputs();
    // map from nodes in the surrounding graph to parameters in the fusion
    // group's subgraph that correspond to them
    std::unordered_map<Node*,Node*> inputs_map;
    size_t i = 0;
    JIT_ASSERT(group->inputs().size() == subgraph.inputs().size());
    for(auto input : group->inputs()) {
      inputs_map[input] = subgraph.inputs()[i++];
    }
    // add n's inputs to the fusion group's input list if we don't already have them
    for(auto input : n->inputs()) {
      if(inputs_map.count(input) == 0) {
        auto in_group = subgraph.addInput();
        in_group->setType(input->typeOption());
        inputs_map[input] = in_group;
        group->addInput(input);
      }
    }
    // copy n into the graph, remapping its inputs to internal nodes
    Node * in_graph = subgraph.createClone(n,[&](Node * k)-> Node* {
      return inputs_map[k];
    });
    // if n is already an input to the fusion group,
    // we need to remove it because n is now inside the fusion group
    // remapping nodes that used the input to the newly-merged node
    // n is not an input when the fusion group is empty
    auto it = std::find(inputs.begin(), inputs.end(), n);
    if(it != inputs.end()) {
      size_t p = it - inputs.begin();
      group->removeInput(p);
      subgraph.inputs()[p]->replaceAllUsesWith(in_graph);
      subgraph.eraseInput(p);
    }
    return subgraph.prependNode(in_graph);
  }

  // turn consumer node n into a fusion group with just n inside
  // to prepare for fusion and replace uses of n with the new group
  FusionGroup * createSingletonFusionGroup(Node * n) {
    auto group = graph->createOld<FusionGroup>();
    // propogate position information for the new node so we can always
    // have a valid mapping
    topological_index[group] = topological_index[n];
    group->insertBefore(n);
    Node * mergedNode = mergeNodeIntoGroup(group,n);
    group->subgraph().registerOutput(mergedNode);
    auto sel = graph->createOld<Select>(group,0);
    sel->setType(n->typeOption());
    sel->insertAfter(group);
    n->replaceAllUsesWith(sel);
    n->destroy();
    return group;
  }
  void insertAfter(Node * n, Node * after) {
    n->insertAfter(after);
    topological_index[n] = topological_index[after];
  }

  FusionGroup * fuse(Node * consumer, Node * producer) {
    auto group = consumer->cast<FusionGroup>();

    if(!group) {
      group = createSingletonFusionGroup(consumer);
    }
    Node * merged = mergeNodeIntoGroup(group, producer);
    // remaining uses of this producer can occur because we allow
    // fusion in cases where uses remain after the consumer
    // if these exist, re-route them to the version of producer
    // created in FusionGroup
    if(producer->uses().size() != 0) {
      size_t offset = group->subgraph().registerOutput(merged);
      Node * new_producer = graph->createOld<Select>(group,offset);
      new_producer->setType(producer->typeOption());
      insertAfter(new_producer, group);
      producer->replaceAllUsesWith(new_producer);
    }
    producer->destroy();
    return group;
  }

  // in places where op can be fused into a consumer but chunk is in the way
  // distribute chunk to op's operands:
  // replace a,b = chunk(op(x,y,z)) with:
  // x0,x1 = chunk(x) (x0 has a's type, x1 has b's type)
  // y0,y1 = chunk(y) (y0 has a's type, y1 has b's type)
  // z0,z1 = chunk(z) (z0 has a's type, z1 has b's type)
  // a = op(x0,y0,z0) (a,b have their same size but are now contiguous)
  // b = op(x1,y1,x1)

  bool tryToMoveChunk(Node * consumer, Node * producer_) {
    // if we are fusing a select,
    Select * producer = producer_->cast<Select>();
    if(!producer)
      return false;
    // and the select refers to a chunk,
    Chunk * chunk = producer->base()->cast<Chunk>();
    if(!chunk)
      return false;
    // and the thing being chunked is fusable into the consumer
    Node * producer_for_chunk = chunk->base();
    if(!isFusable(producer_for_chunk) || !allUsersAreThisConsumer(chunk,producer_for_chunk))
      return false;
    // and all uses of the chunk are in this consumer
    for(auto s : chunk->uses()) {
      for(auto u : s.user->uses()) {
        if(u.user != consumer)
          return false;
      }
    }

    std::vector<Chunk*> chunks;
    for(auto input : producer_for_chunk->inputs()) {
      Chunk * c = graph->createOld<Chunk>(chunk->num_chunks,chunk->dim);
      c->addInput(input);
      insertAfter(c,chunk);
      chunks.push_back(c);
    }

    //as we replace/remove the selects the use list changes, so copy it first
    use_list copy_uses = chunk->uses();
    for(auto s : copy_uses) {
      Select * sel = s.user->cast<Select>();
      size_t i = sel->offset();
      size_t j = 0;
      Node * new_output = graph->createClone(producer_for_chunk,[&](Node * n) {
        auto & c = chunks[j++];
        Select * ns = graph->createOld<Select>(c,i);
        ns->setType(sel->typeOption());
        insertAfter(ns,c);
        return ns;
      });
      if(sel->hasType()) {
        new_output->setType(sel->type()->cast<TensorType>()->contiguous());
      }
      insertAfter(new_output,s.user);
      s.user->replaceAllUsesWith(new_output);
      s.user->destroy();
    }
    chunk->destroy();
    producer_for_chunk->destroy();
    return true;
  }

  // returns where to continue scanning
  graph_node_list::reverse_iterator scanNode(Node * consumer) {
    if(isFusable(consumer)) {
      // handle inputs in reverse topological order as well...
      // otherwise in f(a,a+b) it will appear a is used twice if we consider
      // the f-a fusion before the f-(a+b) fusion first.
      node_list inputs = consumer->inputs();
      for(auto i : inputs) {
        JIT_ASSERT(topological_index.count(i) > 0);
      }
      std::sort(inputs.begin(), inputs.end(), [&](Node * a, Node * b) {
        return topological_index[a] > topological_index[b];
      });
      for(auto producer : inputs) {
        if(tryToMoveChunk(consumer,producer)) {
          // the chunk before this consumer was re-arranged to allow fusion,
          // we scan this consumer again to perform the fusion
          return consumer->reverseIterator();
        }
        if(shouldFuse(consumer, producer)) {
          auto fusion_group = fuse(consumer,producer);
          // after fusion, consumer moves into a FusionGroup, so inputs is no longer valid
          // so we rescan the new FusionGroup for more fusions...
          return fusion_group->reverseIterator();
        }
      }
    }
    return ++consumer->reverseIterator();
  }

  void run() {
    size_t i = 0;
    for(auto p : graph->inputs()) {
      topological_index[p] = i++;
    }
    const auto & nodes = graph->nodes();
    for(auto consumer : nodes) {
      topological_index[consumer] = i++;
    }
    topological_index[graph->return_node()] = i++;
    for(auto it = nodes.rbegin(); it != nodes.rend();) {
      it = scanNode(*it);
    }
  }
};

void FuseGraph(std::unique_ptr<Graph>& graph) {
  GraphFuser(graph).run();
}

}}
