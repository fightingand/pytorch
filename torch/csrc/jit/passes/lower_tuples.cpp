#include "torch/csrc/jit/passes/lower_tuples.h"
#include "torch/csrc/jit/passes/dead_code_elimination.h"
#include "torch/csrc/utils/functional.h"
#include "torch/csrc/jit/assertions.h"

namespace torch { namespace jit {

namespace {

// operators where we expect to find tuples as inputs/outputs
// this is to assert we are only  doing modifications when we know
// we can flatten tuples
std::unordered_set<Symbol> white_list = {
  prim::If,
  prim::Loop,
  prim::TupleUnpack,
  prim::TupleConstruct,
  prim::Param,
  prim::Return,
};

int64_t indexVal(Value * index) {
  return (*toIValue(index->node()->output())).toInt();
}


} //anonymous namespace

static void LowerAllTuples(Block* block);

static void VisitNode(Node* n, Node* insert_point) {
  auto & graph = *n->owningGraph();

  // tuple construction operators will become dead when the unpacks are replaced
  if(n->kind() == prim::TupleConstruct) {
    return;
  }

  // make any TupleUnpack dead by undoing TupleUnpack(TupleConstruct())
  if(n->kind() == prim::TupleUnpack) {
    auto construct = n->input()->node();
    // note: removing these asserts changes this pass from a complete lowering
    // pass to one that removes tuples when possible. When tuples are first-class
    // in the interpreter, we should still run this pass to remove extraneous uses
    JIT_ASSERTM(construct->kind() == prim::TupleConstruct, "tuple unpack not matched to tuple construct");
    for(size_t i = 0; i < n->outputs().size(); ++i) {
      n->outputs()[i]->replaceAllUsesWith(construct->inputs()[i]);
    }
    return;
  }

  // undo TupleUnpack(TupleIndex)
  if (n->kind() == prim::TupleIndex) {
    auto construct = n->inputs().at(0)->node();
    auto idx = indexVal(n->inputs().at(1));
    JIT_ASSERTM(construct->kind() == prim::TupleConstruct, "tuple index not matched to tuple construct");
    n->output()->replaceAllUsesWith(construct->inputs()[idx]);
    return;
  }


  // flatten the input list  op(a, tup, b) --> op(a, t0, t1, b)
  for(size_t i = 0; i < n->inputs().size();) {
    auto input = n->inputs()[i];
    if(TupleTypePtr tt = input->type()->cast<TupleType>()) {
      JIT_ASSERTM(white_list.count(n->kind()) > 0, "tuple appears in op that does not forward tuples");
      JIT_ASSERTM(input->node()->kind() == prim::TupleConstruct, "tuple use not matched to tuple construct");
      for(size_t j = 0; j < tt->elements().size(); ++j) {
        n->insertInput(i + 1 + j, input->node()->inputs().at(j));
      }
      n->removeInput(i);
      // note: no update to i
      // since tuples might be nested we need to recursively scan
      // the new flattened inputs
    } else {
      ++i;
    }
  }
  for(auto b : n->blocks()) {
    LowerAllTuples(b);
  }

  // flatten the outputs list
  for(size_t i = 0; i < n->outputs().size();) {
    Value * output = n->outputs()[i];
    // (a, b, tup, c) -> (a, b, t0, t1, c)
    // and:
    //    tup = (t0, t1)
    // is placed at the current insertion point
    if(TupleTypePtr tt = output->type()->cast<TupleType>()) {
      JIT_ASSERTM(white_list.count(n->kind()) > 0, "tuple appears in op that does not forward tuples");
      for(size_t j = 0; j < tt->elements().size(); j++) {
        n->insertOutput(i + 1 + j)->setType(tt->elements()[j]);
      }
      auto new_tup = graph.createTuple(n->outputs().slice(i + 1, tt->elements().size()));
      new_tup->insertBefore(insert_point);
      insert_point = new_tup;
      output->replaceAllUsesWith(new_tup->output());
      n->eraseOutput(i);
      // note: no update to i to handle nested tuples
    } else {
      ++i;
    }
  }
}

static void LowerAllTuples(Block* block) {
  // tuples in parameter lists of a block behave exactly the same as
  // _outputs_ of normal instructions, since the param_node represents the
  // parameters as outputs, we can handle it by simply visiting the node
  VisitNode(block->param_node(), *block->nodes().begin());
  for(auto it = block->nodes().begin(), end = block->nodes().end(); it != end;) {
    auto n = *it++;
    VisitNode(n, *it);
  }
  // tuples in return lists of blocks behave exactly the same as
  // _inputs_ of normal instructions, so we can use VisitNode here as well
  // insert_point is null because it will never be used since return nodes
  // have no outputs
  VisitNode(block->return_node(), nullptr);
}


static void EnsureNoTuples(ArrayRef<Value*> values) {
  for (Value * v : values) {
    JIT_ASSERTM(v->type()->kind() != TypeKind::TupleType,
                "Couldn't lower all tuples.");
  }
}

static void EnsureNoTuples(Block* block) {
  for (Node* n : block->nodes()) {
    for (Block* b : n->blocks()) {
      EnsureNoTuples(b);
    }
    EnsureNoTuples(n->outputs());
  }
}

void LowerAllTuples(std::shared_ptr<Graph>& graph) {
  LowerAllTuples(graph->block());
  EliminateDeadCode(graph);
  EnsureNoTuples(graph->block());
}

static void LowerSimpleTuples(Block* block) {
  for(auto n : block->nodes()) {
    // make any TupleUnpack dead by undoing TupleUnpack(TupleConstruct())
    // and TupleIndex(TupleIndex)
    if(n->kind() == prim::TupleUnpack) {
      auto construct = n->input()->node();
      if(construct->kind() == prim::TupleConstruct) {
        for(size_t i = 0; i < n->outputs().size(); ++i) {
          n->outputs()[i]->replaceAllUsesWith(construct->inputs()[i]);
        }
      }
    }
    if (n->kind() == prim::TupleIndex) {
      auto construct = n->inputs().at(0)->node();
      if(construct->kind() == prim::TupleConstruct) {
        auto idx = indexVal(n->inputs().at(1));
        n->output()->replaceAllUsesWith(construct->inputs()[idx]);
      }
    }
    for(auto b : n->blocks()) {
      LowerSimpleTuples(b);
    }
  }
}

void LowerSimpleTuples(std::shared_ptr<Graph>& graph) {
  LowerSimpleTuples(graph->block());
  EliminateDeadCode(graph);
}

}}
