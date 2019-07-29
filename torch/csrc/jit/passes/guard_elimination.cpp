#include <torch/csrc/jit/passes/guard_elimination.h>
#include <torch/csrc/jit/graph_executor.h>
#include <torch/csrc/jit/jit_log.h>
#include <torch/csrc/jit/passes/alias_analysis.h>
#include <torch/csrc/jit/passes/constant_propagation.h>
#include <memory>
#include <unordered_set>

namespace torch {
namespace jit {

struct GuardElimination {
  GuardElimination(std::shared_ptr<Graph> graph)
      : graph_(std::move(graph)),
        aliasDb_(c10::guts::make_unique<AliasDb>(graph_)) {}

  void run() {
    moveGuardsToDefs(graph_->block());
    GRAPH_DUMP("After moveGuardsToDefs", graph_);
    coalesceGuards(graph_->block());
    GRAPH_DUMP("After coalesceGuards", graph_);
    runRequiredPasses(graph_);
    ConstantPropagation(graph_);
    eliminateRedundantGuards(graph_->block());
    GRAPH_DUMP("After eliminateRedundantGuards", graph_);
  }

  void moveGuardsToDefs(Block* b) {
    for (auto it = b->nodes().begin(); it != b->nodes().end();) {
      auto n = *it;
      if (n->kind() == prim::Guard) {
        // grab the next node before we move this one all the way back
        it++;
        auto guardee = n->inputs().at(0)->node();
        // alias analysis will try to hoist a node out of a loop
        // if asked. if guardee is in a loop, it should only
        // be moved to the beginning of the basic block
        // given the current implementation of AliasAnalysis
        if (guardee->owningBlock() != n->owningBlock()) {
          guardee = *n->owningBlock()->nodes().begin();
        }
        bool moved = aliasDb_->moveAfterTopologicallyValid(n, guardee);
        if (moved) {
          GRAPH_UPDATE(
              "Moved ",
              n->output()->debugName(),
              " to ",
              n->inputs().at(0)->debugName());
        }
      } else {
        it++;
        for (Block* ib : n->blocks()) {
          moveGuardsToDefs(ib);
        }
      }
    }
  }

  void coalesceGuards(Block* b) {
    // uses on *all* parameters are moved to the same anchor node
    // and they may come in different order after the anchor node
    // e.g. (anchor, guard_x, guard_y, guard_x, guard_y)
    // this pass recognizes contigious streches of guards and
    // keeps track of the guards it's seen for each def. the next time
    // the guard on the same def, it simply removes it.
    std::unordered_map<Value*, Node*> inputs_to_guards;
    for (auto it = b->nodes().begin(); it != b->nodes().end(); it++) {
      auto n = *it;
      if (n->kind() == prim::Guard) {
        if (inputs_to_guards.count(n->input())) {
          auto prev = inputs_to_guards[n->input()];
          n->output()->replaceAllUsesWith(prev->output());
          GRAPH_UPDATE(
              "Replacing ",
              n->output()->debugName(),
              " with ",
              prev->output()->debugName());
          it.destroyCurrent();
        } else {
          inputs_to_guards.insert({n->input(), n});
        }
      } else if (n->kind() != prim::Constant) {
        inputs_to_guards.clear();
        for (Block* ib : n->blocks()) {
          coalesceGuards(ib);
        }
      }
    }
  }

  bool guardsOutput(Node* guard) {
    auto output = guard->input()->node();
    auto it = guard;

    while (it != output) {
      if (it->kind() != prim::Guard && it->kind() != prim::Constant) {
        GRAPH_DEBUG("found an unexpected node ", *it);
        return false;
      }
      it = it->prev();
    }

    return true;
  }

  void eliminateRedundantGuards(Block* b) {
    // a very simple pass to eliminate redundant guards for ops
    // whose outputs are fully determined by their inputs
    // i.e. if inputs to such ops are guarded we are allowed
    // to remove a guard on ops' outputs
    for (auto it = b->nodes().rbegin(); it != b->nodes().rend();) {
      auto n = *it;
      if (n->kind() == prim::Guard && guardsOutput(n) &&
          removableGuard(n->inputs().at(0)->node())) {
        auto pttp = n->output()->type();
        n->output()->replaceAllUsesWith(n->inputs().at(0));
        n->inputs().at(0)->setType(pttp);
        GRAPH_UPDATE(
            "Eliminating the redundant guard ", n->output()->debugName());
        it.destroyCurrent();
      } else {
        it++;
        for (Block* ib : n->blocks()) {
          eliminateRedundantGuards(ib);
        }
      }
    }
  }

  bool checkInputs(Node* n, const std::unordered_set<size_t>& except) {
    bool all_inputs_guarded = true;
    size_t i = 0;
    for (auto input : n->inputs()) {
      if (input->node()->kind() == prim::Guard ||
          input->node()->kind() == prim::Constant || except.count(i) != 0) {
        AT_ASSERT(
            input->node()->kind() != prim::Guard ||
            input->type()->expect<ProfiledTensorType>());
      } else {
        GRAPH_DEBUG("input ", input->debugName(), " isn't guarded");
        all_inputs_guarded = false;
        break;
      }
      i++;
    }
    return all_inputs_guarded;
  }

 private:
  bool removableGuard(Node* n) {
    const static auto no_exceptions = std::unordered_set<size_t>{};
    switch (n->kind()) {
      case aten::add:
      case aten::sub:
      case aten::mul:
      case aten::div:
      case aten::t:
      case aten::sigmoid:
      case aten::tanh:
      case aten::mm:
      case aten::min:
      case aten::max:
      case prim::ConstantChunk:
        return checkInputs(n, no_exceptions);
      case aten::clamp:
        // the second and third args do not affect shapes
        return checkInputs(n, std::unordered_set<size_t>{1, 2});
      default:
        GRAPH_DEBUG("cannot remove ", n->kind().toQualString());
        return false;
    }
  }

  std::shared_ptr<Graph> graph_;
  std::unique_ptr<AliasDb> aliasDb_;
  static std::unordered_set<Symbol> simple_ops_;
};

static void removeProfilingNodes(Block* b) {
  for (auto it = b->nodes().begin(); it != b->nodes().end(); it++) {
    if (it->kind() == prim::profile) {
      it.destroyCurrent();
    } else {
      for (Block* ib : it->blocks()) {
        removeProfilingNodes(ib);
      }
    }
  }
}

void EliminateRedundantGuards(std::shared_ptr<Graph> graph) {
  GuardElimination ge(std::move(graph));
  ge.run();
}

} // namespace jit
} // namespace torch
