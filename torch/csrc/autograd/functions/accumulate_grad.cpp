#include <torch/csrc/autograd/functions/accumulate_grad.h>

#include <torch/csrc/autograd/grad_mode.h>
#include <torch/csrc/autograd/variable.h>
#include <torch/csrc/autograd/functions/basic_ops.h>
#include <torch/csrc/autograd/functions/tensor.h>
#include <torch/csrc/autograd/functions/utils.h>

#include <cstdint>
#include <stdexcept>
#include <utility>

using at::Tensor;

namespace torch { namespace autograd {

// AccumulateGrad sets sequence_nr to the max value so it's always called
// ASAP during backwards.
AccumulateGrad::AccumulateGrad(const Variable& variable_)
    : Node(/*sequence_nr=*/UINT64_MAX)
    , variable(variable_)
    , variable_grad(variable_.grad()) {
  add_input_metadata(variable_);
}

auto AccumulateGrad::apply(variable_list&& grads) -> variable_list {
  // XXX: this method is not thread-safe!
  check_input_variables("AccumulateGrad", grads, 1, 0);

  if (!grads[0].defined())
    return {};

  auto var = variable.lock();
  // It's possible that the Variable went out of scope and was freed.
  // We still need to handle the unlikely case of someone holding to its grad.
  if (!var.defined()) {
    auto var_grad = variable_grad.lock();
    // Everything was freed. Nothing to do.
    if (!var_grad.defined()) return variable_list();
    // Now here's the hard part. If both the new_grad and var_grad require grad
    // then we just accumulate the data in place (as we'd do if the Variable was
    // alive). Otherwise, we'd need to perform the out-of-place reduction, but
    // since the user only holds a reference to .grad and there's no way to
    // give him the new Value, we just assume that they know these attributes
    // are changing when using higher order graphs.
    if (GradMode::is_enabled() && var_grad.requires_grad() && grads[0].requires_grad()) {
      var_grad += grads[0];
    }
    return variable_list();
  }

  if (var.grad_fn())
    throw std::logic_error("leaf variable has been moved into the graph interior");
  if (!var.requires_grad())
    return {};

  auto new_grad = std::move(grads[0]);
  for (auto& hook : var.hooks()) {
    new_grad = (*hook)({new_grad})[0];
  }

  at::Tensor& grad = var.grad();
  if (!grad.defined()) {
    // under following condition, we can avoid clone()
    if (!GradMode::is_enabled()
        && !new_grad.is_sparse()
        && new_grad.is_contiguous()
        && new_grad.use_count() <= 1 + !post_hooks().empty()) {
      // first check it is in first-order grad only mode
      // then check not sparse before is_contiguous
      // then check contiguous, otherwise later in place accumulation may fail
      // and lastly, check it is the last reference before we grab it.
      // If the function has post hooks (for example, a DDP allreduce hook),
      // call_function in Engine.cpp will temporarily bump the refcount by one, hence the
      // addition of !post_hooks().empty().
      var.grad() = new_grad.detach();
    } else {
      var.grad() = new_grad.clone();
    }
    variable_grad = WeakVariable(var.grad()); // We need to update our reference
    // This case is not strictly necessary, but it makes the first-order only case
    // slightly more efficient and, what's more important, more predictable for
    // the users. Thanks to this case we can avoid changing the grad tensor,
    // a thing never promised and documented, but used in some hacks seen
    // on the internet.
  } else if (!GradMode::is_enabled()) {
    // This case is not strictly necessary, but it makes the first-order only case
    // slightly more efficient.
    Variable& grad_variable = as_variable_ref(grad);
    if (grad_variable.is_sparse() && !new_grad.is_sparse()) {
      // If `grad_variable` is sparse and `new_grad` is not sparse, their sum is not
      // sparse, and we must change the TensorImpl type of `grad_variable` for it to
      // store the result. However, changing the TensorImpl type of a tensor requires
      // changing the tensor itself, and thus in this case we have to change the grad
      // tensor.
      grad_variable = new_grad + grad_variable;
    } else {
      // In this case we can avoid changing the grad tensor. There are three scenarios
      // when we'll hit this case:
      //
      // 1. `grad_variable` is sparse, and `new_grad` is sparse.
      // 2. `grad_variable` is dense, and `new_grad` is sparse.
      // 3. `grad_variable` is dense, and `new_grad` is dense.
      //
      // In all of these three cases, `grad_variable += new_grad` is a valid operation
      // which adds `new_grad` to `grad_variable` in place. `grad_variable` is thus
      // still referring to the same tensor after the operation.
      grad_variable += new_grad;
    }
  } else {
    var.grad() = grad + new_grad;
  }

  return variable_list();
}

}} // namespace torch::autograd
