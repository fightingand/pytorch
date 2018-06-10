#pragma once

#include <memory>
#include "torch/csrc/jit/ir.h"
#include "torch/csrc/jit/variable_tensor_list.h"
#include "torch/csrc/jit/interpreter.h"
#include "torch/csrc/jit/autodiff.h"
#include "torch/csrc/jit/argument_spec.h"

namespace torch { namespace jit {

struct GraphExecutorState;

// Notice that those structs don't manage lifetime of their members.
// They is only valid only right after you call getDebugState() and should never
// be used again once another GraphExecutor function is called.
struct ExecutionPlanState {
  Code* f;
  Graph* graph;

  // Those two fields are optional
  Gradient* grad;
  std::shared_ptr<GraphExecutorState> grad_executor; // shared_ptr to break the cycle...
};

struct GraphExecutorState {
  Graph* graph;
  std::unordered_map<ArgumentSpec, ExecutionPlanState> execution_plans;

  // Those two fields are optional
  Code* autograd_fallback;
  Graph* autograd_fallback_graph;
};

struct GraphExecutorImpl;
struct GraphExecutor {
  GraphExecutor() {}
  GraphExecutor(std::shared_ptr<Graph> graph, bool optimize = true);
  // note: if not specified, symbolically_differentiable is computed from the graph.
  GraphExecutor(std::shared_ptr<Graph> graph, bool optimize, bool symbolically_differentiable);
  variable_tensor_list run(variable_tensor_list && inputs);
  operator bool() const {
    return pImpl != nullptr;
  }
  std::shared_ptr<Graph> graph() const;
  std::shared_ptr<Graph> graphFor(const variable_tensor_list& inputs) const;
  GraphExecutorState getDebugState();
private:
  std::shared_ptr<GraphExecutorImpl> pImpl;
};

}}
