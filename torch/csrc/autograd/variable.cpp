#include <torch/csrc/autograd/variable.h>

#include <torch/csrc/autograd/InferenceMode.h>
#include <torch/csrc/autograd/autograd.h>
#include <torch/csrc/autograd/edge.h>
#include <torch/csrc/autograd/engine.h>
#include <torch/csrc/autograd/function.h>
#include <torch/csrc/autograd/functions/accumulate_grad.h>
#include <torch/csrc/autograd/functions/tensor.h>
#include <torch/csrc/autograd/generated/Functions.h>
#include <torch/csrc/autograd/utils/error_messages.h>

#include <ATen/core/VariableHooksInterface.h>

#include <ATen/ATen.h>
#include <ATen/FuncTorchTLS.h>
#include <ATen/MemoryOverlap.h>
#include <c10/util/Exception.h>

#include <iostream>
#include <list>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <typeinfo>
#include <vector>

namespace torch {
namespace autograd {

DifferentiableViewMeta::DifferentiableViewMeta(
    at::TensorImpl* self_impl,
    c10::optional<ViewInfo> backward_info,
    c10::optional<ViewInfo> forward_info,
    bool shared_view_info,
    CreationMeta creation_meta)
    : AutogradMeta(self_impl),
      backward_info_(std::move(backward_info)),
      forward_info_(std::move(forward_info)),
      shared_view_info_(shared_view_info),
      creation_meta_(creation_meta) {
  is_view_ = true;
  if (backward_info_.has_value()) {
    self_impl->set_version_counter(
        impl::version_counter(backward_info_.value().base_));
    attr_version_ = self_impl->version_counter().current_version();
  }
  if (shared_view_info_) {
    TORCH_INTERNAL_ASSERT(
        backward_info_.has_value(),
        "Shared view info require a backward view info.");
    TORCH_INTERNAL_ASSERT(
        !forward_info_.has_value(),
        "Shared view info require forward view info to be empty")
  }
}

// Chain this view info with the new view op between base and tensor
ViewInfo ViewInfo::chain(
    const Variable& base,
    const Variable& tensor,
    std::function<Variable(const Variable&)> view_func) const {
  // Set `view_func` using the root base as input.
  // `view_func` is used to recover views in backward when either as_strided is
  // not supported or the view function changes the metadata which is not
  // recorded by as_strided See Note [View + Inplace update on base tensor] and
  // [View + Inplace update on view tensor] for more details how we use this
  // function in backward.
  if (view_func) {
    // both current_view and it's parent have a view_func
    if (view_fn_) {
      // Copy parent view function to gain ownership
      auto prev_fn = view_fn_;
      view_func = [=](const at::Tensor& root_base) {
        auto temp = prev_fn(root_base);
        return view_func(temp);
      };
    } else {
      // current_view has a view_func and but it's parent doesn't have one
      if (base.unsafeGetTensorImpl()->support_as_strided()) {
        auto size = base.sizes().vec();
        auto stride = base.strides().vec();
        auto storage_offset = base.storage_offset();
        view_func = [=](const at::Tensor& root_base) {
          auto temp = root_base.as_strided(size, stride, storage_offset);
          return view_func(temp);
        };
      } else {
        // When base is a view but doesn't carry a view_fn in
        // DifferentiableViewMeta, it's a view that doesn't support inplace
        // update, e.g. unbind. In this case we should throw an error when
        // inplace update happens in **forward**. One would naturally think the
        // following function will be first called in backward pass. But the
        // first call site is indeed in **forward** pass when we refresh
        // `grad_fn` triggered by inplace update. Search Note [View + Inplace
        // update for view tensor] to for the call site.
        view_func = [=](const at::Tensor& root_base) {
          TORCH_CHECK(
              false,
              "This view is the output of a function that returns multiple views."
              "Such functions do not allow the output views to be modified inplace."
              "You should replace the inplace operation by an out-of-place one");
          return root_base;
        };
      }
    }
  } else if (view_fn_) {
    // if current_view doesn't have a view_func but it's parent has one
    // Copy parent view function to gain ownership
    auto prev_view_fn = view_fn_;
    auto size = tensor.sizes().vec();
    auto stride = tensor.strides().vec();
    auto storage_offset = tensor.storage_offset();
    view_func = [=](const at::Tensor& root_base) {
      auto temp = prev_view_fn(root_base);
      return temp.as_strided(size, stride, storage_offset);
    };
  }

  return ViewInfo(base_, view_func);
}

namespace {

at::Tensor singleton_undefined_tensor;

struct ConcreteAutogradMetaFactory : public c10::impl::AutogradMetaFactory {
  std::unique_ptr<c10::AutogradMetaInterface> make() const override {
    return std::make_unique<AutogradMeta>();
  }
  const at::Tensor& undefined_tensor() const override {
    return singleton_undefined_tensor;
  }
};

ConcreteAutogradMetaFactory meta_factory;

static c10::impl::AutogradMetaFactoryRegisterer meta_factory_registerer(
    &meta_factory);

} // namespace

namespace impl {

AutogradMeta* materialize_autograd_meta(const at::TensorBase& self) {
  TORCH_CHECK(
      self.defined(),
      "cannot call materialize_autograd_meta() on undefined tensor");
  auto p = self.unsafeGetTensorImpl();
  if (!p->autograd_meta()) {
    p->set_autograd_meta(std::make_unique<AutogradMeta>());
  }
  return get_autograd_meta(self);
}

void update_cpp_hooks_on_new_gradfn(
    const at::TensorBase& self,
    const std::shared_ptr<torch::autograd::Node>& new_fn) {
  // This function is called whenever the grad_fn of the tensor is
  // changed. We assume here that new_fn does not yet have hooks of
  // its own
  //
  // This function does two things:
  const auto& meta = impl::get_autograd_meta(self);
  TORCH_INTERNAL_ASSERT(meta);
  TORCH_INTERNAL_ASSERT(new_fn);
  if (!self.retains_grad()) {
    // (1) reset the list when grad_fn is updated, so new hooks don't
    //     get erroneously registered to the old grad_fn.
    //     Note that the old cpp_hooks_list_ is still kept alive by the
    //     old grad_fn so hooks registered to the older version of the tensor
    //     will continue to be active.
    meta->cpp_hooks_list_ = nullptr;
    return;
  }
  // (2) If there is a retains_grad hook registered, move that from the
  //     old cpp_hooks_list_ to the new one
  auto idx = meta->retains_grad_;
  auto new_list = std::make_shared<hooks_list>();
  new_list->push_back(std::move((*meta->cpp_hooks_list_)[idx]));
  (*meta->cpp_hooks_list_)[idx] = nullptr;
  meta->cpp_hooks_list_ = new_list;
  // Since this is a new list, 0 is the index of the retains_grad hook
  meta->retains_grad_ = 0;
  std::unique_ptr<FunctionPreHook> hook_ptr(
      new CppFunctionPreHook(meta->cpp_hooks_list_, self.output_nr()));
  new_fn->add_pre_hook(std::move(hook_ptr));
}

void rebase_history(const Variable& self, Edge gradient_edge) {
  TORCH_INTERNAL_ASSERT(gradient_edge.function != nullptr);
  auto diff_view_meta = get_view_autograd_meta(self);
  if (diff_view_meta && diff_view_meta->has_bw_view()) {
    // See NOTE [ View + Inplace detection ]
    auto creation_meta = diff_view_meta->get_creation_meta();
    // Do not use handle_view_on_rebase here as check_inplace should have been
    // called before this and either throw an error
    TORCH_INTERNAL_ASSERT(creation_meta == CreationMeta::DEFAULT);
    TORCH_INTERNAL_ASSERT(gradient_edge.input_nr == 0);
    TORCH_INTERNAL_ASSERT(gradient_edge.function);
    TORCH_CHECK(
        gradient_edge.function->num_inputs() == 1,
        "Functions which modify views in-place must return a single Variable");
    auto view_info = diff_view_meta->get_backward_view();
    diff_view_meta->output_nr_ = gradient_edge.input_nr;
    auto copy_slices = std::make_shared<CopySlices>(
        view_info.base_,
        at::TensorGeometry(self),
        view_info.view_fn_,
        std::move(gradient_edge.function));
    set_gradient_edge(view_info.base_, {std::move(copy_slices), 0});
    self.grad_fn(); // trigger an update to the view's grad_fn
    return;
  }

  set_gradient_edge(self, std::move(gradient_edge));
  // Pass both self and its grad_fn to avoid calling into grad_fn reentrantly
  torch::autograd::impl::update_cpp_hooks_on_new_gradfn(self, self.grad_fn());
}

void create_cpp_hook(const at::TensorBase& self) {
  auto& list = materialize_autograd_meta(self)->cpp_hooks_list_;
  // NOLINTNEXTLINE(modernize-make-shared)
  list.reset(new hooks_list());
  std::unique_ptr<FunctionPreHook> hook_ptr(
      new CppFunctionPreHook(list, self.output_nr()));
  clear_hooks(self);
  add_hook(self, std::make_shared<CppFunctionPreHook>(list, 0));
  const auto& fn = self.grad_fn();
  if (fn) {
    fn->add_pre_hook(std::move(hook_ptr));
  }
}

void set_grad_accumulator(
    const Variable& self,
    std::weak_ptr<Node> grad_accumulator) {
  materialize_autograd_meta(self)->grad_accumulator_ =
      std::move(grad_accumulator);
}

std::shared_ptr<Node> try_get_grad_accumulator(const Variable& self) {
  if (get_autograd_meta(self)) {
    return get_autograd_meta(self)->grad_accumulator_.lock();
  } else {
    return nullptr;
  }
}

std::shared_ptr<Node> grad_accumulator(const Variable& self) {
  auto autograd_meta = get_autograd_meta(self);
  if (!autograd_meta) {
    return nullptr;
  }
  if (autograd_meta->grad_fn_) {
    throw std::logic_error(
        "grad_accumulator() should be only called on leaf Variables");
  }
  if (!autograd_meta->requires_grad_) {
    return nullptr;
  }

  std::lock_guard<std::mutex> lock(autograd_meta->mutex_);

  auto result = autograd_meta->grad_accumulator_.lock();
  if (result)
    return result;

  c10::raw::intrusive_ptr::incref(self.unsafeGetTensorImpl());
  auto intrusive_from_this =
      c10::intrusive_ptr<at::TensorImpl>::reclaim(self.unsafeGetTensorImpl());
  result = std::make_shared<AccumulateGrad>(
      Variable(std::move(intrusive_from_this)));
  autograd_meta->grad_accumulator_ = result;
  return result;
}

Edge gradient_edge(const Variable& self) {
  // If grad_fn is null (as is the case for a leaf node), we instead
  // interpret the gradient function to be a gradient accumulator, which will
  // accumulate its inputs into the grad property of the variable. These
  // nodes get suppressed in some situations, see "suppress gradient
  // accumulation" below. Note that only variables which have `requires_grad =
  // True` can have gradient accumulators.
  if (const auto& gradient = self.grad_fn()) {
    return Edge(gradient, self.output_nr());
  } else {
    return Edge(grad_accumulator(self), 0);
  }
}

void set_gradient_edge(const Variable& self, Edge edge) {
  auto* meta = materialize_autograd_meta(self);
  meta->grad_fn_ = std::move(edge.function);
  meta->output_nr_ = edge.input_nr;
  // For views, make sure this new grad_fn_ is not overwritten unless it is
  // necessary in the VariableHooks::grad_fn below. This logic is only relevant
  // for custom autograd Functions for which multiple operations can happen on a
  // given Tensor before its gradient edge is set when exiting the custom
  // Function.
  auto diff_view_meta = get_view_autograd_meta(self);
  if (diff_view_meta && diff_view_meta->has_bw_view()) {
    diff_view_meta->set_attr_version(self._version());
  }
}

Node* grad_fn_unsafe(const Variable& self) {
  if (get_autograd_meta(self)) {
    return get_autograd_meta(self)->grad_fn_.get();
  } else {
    return nullptr;
  }
}

// Versions
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

void set_version_counter(
    const Variable& self,
    const c10::VariableVersion& version_counter) {
  TORCH_CHECK(
      self.defined(), "cannot call set_version_counter() on undefined tensor");
  self.unsafeGetTensorImpl()->set_version_counter(version_counter);
}

void bump_version(const Variable& self) {
  TORCH_CHECK(self.defined(), "cannot call bump_version() on undefined tensor");
  self.unsafeGetTensorImpl()->bump_version();
}

const c10::VariableVersion& version_counter(const Variable& self) {
  TORCH_CHECK(
      self.defined(), "cannot call version_counter() on undefined tensor");
  return self.unsafeGetTensorImpl()->version_counter();
}

// Hooks
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

void add_hook(
    const at::TensorBase& self,
    std::shared_ptr<FunctionPreHook> hook) {
  materialize_autograd_meta(self)->hooks_.push_back(std::move(hook));
}

namespace {
std::vector<std::shared_ptr<FunctionPreHook>> empty_singleton;
}

// TODO: Return an ArrayRef instead (and delete the singleton while you're at
// it
const std::vector<std::shared_ptr<FunctionPreHook>>& hooks(
    const Variable& self) {
  if (get_autograd_meta(self)) {
    return get_autograd_meta(self)->hooks_;
  } else {
    return empty_singleton;
  }
}

void clear_hooks(const at::TensorBase& self) {
  // This is a little goofy, but usually this should be a no oop
  materialize_autograd_meta(self)->hooks_.clear();
}

void set_name(const Variable& self, const std::string& name) {
  materialize_autograd_meta(self)->name_ = name;
}

// Miscellaneous
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

AutogradMeta* get_autograd_meta(const at::TensorBase& self) {
  // NB: could return nullptr
  TORCH_CHECK(
      self.defined(), "cannot call get_autograd_meta() on undefined tensor");
  return static_cast<AutogradMeta*>(
      self.unsafeGetTensorImpl()->autograd_meta());
}

DifferentiableViewMeta* get_view_autograd_meta(const at::TensorBase& self) {
  // NB: return nullptr if self is not a view
  AutogradMeta* meta = get_autograd_meta(self);
  if (meta && meta->is_view_) {
    return static_cast<DifferentiableViewMeta*>(meta);
  } else {
    return nullptr;
  }
}

} // namespace impl

using at::Tensor;

struct VariableHooks final : at::impl::VariableHooksInterface {
  at::TensorBase tensor_data(const at::TensorBase&) const override;
  at::TensorBase variable_data(const at::TensorBase&) const override;
  const std::shared_ptr<torch::autograd::Node>& grad_fn(
      const at::TensorBase&) const override;
  unsigned _register_hook(
      const at::TensorBase&,
      std::function<at::TensorBase(const at::TensorBase&)> hook) const override;
  void remove_hook(const at::TensorBase&, unsigned pos) const override;
  bool is_view(const at::TensorBase&) const override;
  const at::TensorBase& base(const at::TensorBase&) const override;
  const std::string& name(const at::TensorBase&) const override;
  bool is_leaf(const at::TensorBase&) const override;
  int64_t output_nr(const at::TensorBase&) const override;
  void set_data(const at::TensorBase& self, const at::TensorBase& new_data)
      const override;
  at::TensorBase data(const at::TensorBase& self) const override;
  int64_t _version(const at::TensorBase& self) const override;
  void retain_grad(const at::TensorBase& self) const override;
  bool retains_grad(const at::TensorBase& self) const override;
  void _backward(
      const Tensor& self,
      at::TensorList inputs,
      const c10::optional<Tensor>& gradient,
      c10::optional<bool> keep_graph,
      bool create_graph) const override;
  void requires_grad_(const at::TensorBase& self, bool _requires_grad)
      const override;
};

VariableHooks variableHooks;
at::impl::VariableHooksRegisterer registerVariableHooks(&variableHooks);

at::TensorBase VariableHooks::variable_data(const at::TensorBase& self) const {
  TORCH_CHECK(
      self.defined(), "cannot call variable_data() on undefined tensor");
  auto self_impl_copy = self.unsafeGetTensorImpl()->shallow_copy_and_detach(
      /*version_counter=*/0,
      /*allow_tensor_metadata_change=*/false);
  self_impl_copy->set_autograd_meta(nullptr);
  return at::Tensor(self_impl_copy);
}

at::TensorBase VariableHooks::tensor_data(const at::TensorBase& self) const {
  TORCH_CHECK(self.defined(), "cannot call tensor_data() on undefined tensor");
  auto self_impl_copy = self.unsafeGetTensorImpl()->shallow_copy_and_detach(
      /*version_counter=*/self.unsafeGetTensorImpl()->version_counter(),
      /*allow_tensor_metadata_change=*/
      self.unsafeGetTensorImpl()->allow_tensor_metadata_change());
  return at::Tensor(self_impl_copy);
}

bool VariableHooks::is_leaf(const at::TensorBase& self) const {
  if (impl::get_autograd_meta(self)) {
    return impl::get_autograd_meta(self)->grad_fn_ == nullptr;
  } else {
    return true;
  }
}

int64_t VariableHooks::output_nr(const at::TensorBase& self) const {
  if (impl::get_autograd_meta(self)) {
    return impl::get_autograd_meta(self)->output_nr_;
  } else {
    return 0;
  }
}

void VariableHooks::set_data(
    const at::TensorBase& self_base,
    const at::TensorBase& new_data_base) const {
  at::OptionalTensorRef self_ref(self_base);
  const Tensor& self = *self_ref;
  at::OptionalTensorRef new_data_ref(new_data_base);
  const Tensor& new_data = *new_data_ref;

  // `var.set_data(new_data)` shallow-copies all non-autograd TensorImpl fields
  // from `new_data` to `var`. It requires that `new_data` and `var` have
  // compatible tensor type.
  TORCH_CHECK(
      _has_compatible_shallow_copy_type(self, new_data),
      "Attempted to call `variable.set_data(tensor)`, but `variable` and `tensor` have incompatible tensor type.");

  TORCH_CHECK(
      !self.requires_grad() ||
          isDifferentiableType(at::typeMetaToScalarType(new_data.dtype())),
      "data set to a tensor that requires gradients must be floating point or complex dtype");

  // Resets gradient accumulator if metadata is out of date
  AutogradMeta* autograd_meta = impl::get_autograd_meta(self);
  if (autograd_meta) {
    std::lock_guard<std::mutex> lock(autograd_meta->mutex_);
    auto prior_accumulator = autograd_meta->grad_accumulator_.lock();
    if (prior_accumulator) {
      const auto prior_device = prior_accumulator->input_metadata(0).device();
      const auto new_device = new_data.device();

      if (!new_data.options().type_equal(self.options()) ||
          prior_device != new_device) {
        autograd_meta->grad_accumulator_.reset();
      }
    }
  }

  // Version counter is not shared when we replace a `Variable`'s tensor data
  // by calling `set_data(...)`. The original version of the `Variable` is
  // always preserved. See NOTE [ Version Counter Sharing ] for details.
  //
  // `var.set_data(new_data)` always ignores `var`'s
  // `allow_tensor_metadata_change_`, because users need this API as an escape
  // hatch for changing a tensor's metadata regardless of its
  // `allow_tensor_metadata_change_` value, and the users are responsible for
  // ensuring this is the behavior they want.
  self.unsafeGetTensorImpl()->shallow_copy_from(new_data.getIntrusivePtr());
}

at::TensorBase VariableHooks::data(const at::TensorBase& self) const {
  return self.variable_data();
}

int64_t VariableHooks::_version(const at::TensorBase& self) const {
  return self.unsafeGetTensorImpl()->version_counter().current_version();
}

void VariableHooks::retain_grad(const at::TensorBase& self) const {
  TORCH_CHECK(
      self.requires_grad(),
      "can't retain_grad on Tensor that has requires_grad=False");

  // temporary hack to improve functorch UX.
  const auto& functorch_tls = at::functorch::functorchTLSAccessor();
  if (functorch_tls) {
    functorch_tls->checkSupportsRetainGrad();
  }

  if (self.is_leaf()) { // no-op for leaves
    return;
  }
  if (impl::get_autograd_meta(self)->retains_grad_ != -1) {
    return;
  }
  c10::weak_intrusive_ptr<c10::TensorImpl> weak_self(self.getIntrusivePtr());

  auto retain_grad_hook = [weak_self](const at::Tensor& grad) {
    if (weak_self.expired()) {
      return;
    } else {
      auto var = weak_self.lock();
      if (!var->grad().defined()) {
        if (grad.is_sparse()) {
          var->mutable_grad() = grad.clone();
        } else {
          var->mutable_grad() = grad.clone(at::MemoryFormat::Contiguous);
        }
      } else {
        var->mutable_grad() = var->grad() + grad;
      }
    }
  };

  auto idx = at::OptionalTensorRef(self)->register_hook(retain_grad_hook);
  impl::get_autograd_meta(self)->retains_grad_ = idx;
}

bool VariableHooks::retains_grad(const at::TensorBase& self) const {
  if (impl::get_autograd_meta(self)) {
    return impl::get_autograd_meta(self)->retains_grad_ != -1;
  } else {
    return false;
  }
}

void VariableHooks::_backward(
    const Tensor& self,
    at::TensorList inputs,
    const c10::optional<Tensor>& gradient,
    c10::optional<bool> keep_graph,
    bool create_graph) const {
  // TODO torch::autograd::backward should take the c10::optional<Tensor>
  // gradient directly instead of us having to unwrap it to Tensor _gradient
  // here.
  Tensor _gradient = gradient.has_value() ? *gradient : Tensor();
  std::vector<torch::autograd::Variable> input_vars(
      inputs.begin(), inputs.end());
  torch::autograd::backward(
      {self}, {_gradient}, keep_graph, create_graph, input_vars);
}

void VariableHooks::requires_grad_(
    const at::TensorBase& self,
    bool _requires_grad) const {
  if (!self.is_leaf() && !_requires_grad) {
    throw std::runtime_error(
        autograd::utils::requires_grad_leaf_error(_requires_grad));
  }
  self.set_requires_grad(_requires_grad);
}

// Backward View Variables
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

bool VariableHooks::is_view(const at::TensorBase& self) const {
  auto diff_view_meta = torch::autograd::impl::get_view_autograd_meta(self);
  if (diff_view_meta) {
    return diff_view_meta->has_bw_view();
  } else {
    return false;
  }
}

const at::TensorBase& VariableHooks::base(const at::TensorBase& self) const {
  auto diff_view_meta = torch::autograd::impl::get_view_autograd_meta(self);
  if (diff_view_meta) {
    TORCH_CHECK(
        diff_view_meta->has_bw_view(),
        "Can't get base of non-backward view Tensor");
    return diff_view_meta->get_backward_view().base_;
  } else {
    throw std::runtime_error("Can't get base of non-view Tensor");
  }
}

namespace {
std::string singleton_string;
}

const std::string& VariableHooks::name(const at::TensorBase& self) const {
  TORCH_CHECK(
      self.defined(), "cannot call variable_data() on undefined tensor");
  if (torch::autograd::impl::get_autograd_meta(self)) {
    return torch::autograd::impl::get_autograd_meta(self)->name_;
  } else {
    return singleton_string;
  }
}

namespace {
std::shared_ptr<torch::autograd::Node> singleton_shared_ptr;
}

const std::shared_ptr<torch::autograd::Node>& VariableHooks::grad_fn(
    const at::TensorBase& self) const {
  auto diff_view_meta = torch::autograd::impl::get_view_autograd_meta(self);
  if (diff_view_meta && diff_view_meta->has_bw_view()) {
    // See NOTE [ View + Inplace detection ]
    std::lock_guard<std::mutex> lock(diff_view_meta->mutex_);
    auto view_info = diff_view_meta->get_backward_view();
    if (!diff_view_meta->grad_fn_ && !view_info.base_.requires_grad()) {
      return diff_view_meta->grad_fn_;
    }
    auto current_version = self._version();
    if (diff_view_meta->get_attr_version() != current_version) {
      // This is an indirect rebase_history due to another view or the base
      // being modified inplace
      handle_view_on_rebase(diff_view_meta, /* indirect */ true);
      TORCH_INTERNAL_ASSERT(diff_view_meta->output_nr_ == 0);
      // Note [View + Inplace update for view tensor]
      // An inplace update happened on Tensor `self` (which is a view).
      // For example:
      //   view_1 = view_op_1(diff_view_meta->base_)
      //   view_2 = view_op_2(view_1)
      //   ...
      //   self = view_op_n(view_n-1)
      //   self = inplace_op(self)
      //
      // For CPU/CUDA backends, we employ one AsStridedBackward0 Node to
      // represent the chain of view backward ops for effienciency.
      //
      // However in XLA backend we don't have full support of
      // AsStridedBackward0, we instead run a full forward pass with a tensor
      // that requires gradient to get proper grad_fn setup, then save it to
      // DifferentiableViewMeta for future use. This is fairly cheap for XLA
      // lazy tensor approach (but would be really expensive for CPU/CUDA). XLA
      // Tensor only run thorugh VariableType dispatch and lower the forward
      // pass to a XLA HLO graph, then we take grad_fn and never materialize the
      // tensor content. So we only construct the graph but not execute it,
      // which is a fairly cheap operation to do.
      //
      // See Note [View + Inplace update for base tensor] for what we do to base
      // tensor when an in-place operation happens.
      //
      // TODO: Potentially the following logic can be replaced by special logic
      // in VariableType_x.cpp
      //       that would provide a way to recreate the grad_fn chain.
      if (view_info.has_view_fn()) {
        auto view_fn = view_info.view_fn();
        auto diff_view = view_fn(view_info.base_);
        diff_view_meta->grad_fn_ = diff_view.grad_fn();
      } else {
        auto fn =
            std::make_shared<torch::autograd::generated::AsStridedBackward0>();
        fn->self_geometry = at::TensorGeometry(view_info.base_);
        fn->size = self.sizes().vec();
        fn->stride = self.strides().vec();
        fn->storage_offset = self.storage_offset();
        fn->set_next_edges(
            torch::autograd::collect_next_edges(view_info.base_));
        fn->add_input_metadata(
            view_info.base_.options(),
            self.sizes(), // Note: sizes(), not base_.sizes(), is intentional
            self.unsafeGetTensorImpl()->is_python_dispatch());
        diff_view_meta->grad_fn_ = std::move(fn);
      }
      diff_view_meta->set_attr_version(current_version);

      torch::autograd::impl::update_cpp_hooks_on_new_gradfn(
          self, diff_view_meta->grad_fn_);
    }
    return diff_view_meta->grad_fn_;
  }

  if (torch::autograd::impl::get_autograd_meta(self)) {
    return torch::autograd::impl::get_autograd_meta(self)->grad_fn_;
  } else {
    return singleton_shared_ptr;
  }
}

void VariableHooks::remove_hook(const at::TensorBase& self, unsigned pos)
    const {
  auto& list =
      torch::autograd::impl::materialize_autograd_meta(self)->cpp_hooks_list_;
  TORCH_CHECK(
      list && pos < list->size(), "Invalid index, no hook at position ", pos);
  // Hook will be ignored
  (*list)[pos] = nullptr;
}

unsigned VariableHooks::_register_hook(
    const at::TensorBase& self,
    std::function<at::TensorBase(const at::TensorBase&)> hook) const {
  TORCH_CHECK(
      self.requires_grad(),
      "cannot register a hook on a variable that "
      "doesn't require gradient");
  // NB: materialize_autograd_meta unnecessary due to requires grad check
  auto& list = torch::autograd::impl::get_autograd_meta(self)->cpp_hooks_list_;
  if (!list) {
    torch::autograd::impl::create_cpp_hook(self);
  }
  unsigned idx = list->size();
  list->push_back(hook);
  return idx;
}

void handle_view_on_rebase(
    DifferentiableViewMeta* diff_view_meta,
    bool indirect) {
  /// See NOTE [ View + Inplace detection ] for justification of the logic below
  auto creation_meta = diff_view_meta->get_creation_meta();
  if (creation_meta != CreationMeta::DEFAULT) {
    auto grad_fn = diff_view_meta->grad_fn_.get();
    std::string msg;
    std::string modified_obj;
    // Create the header for the error message.
    if (indirect) {
      modified_obj = "its base or another view of its base has been";
    } else {
      modified_obj = "is being";
    }
    if (grad_fn) {
      msg = c10::str(
          "Output ",
          diff_view_meta->output_nr_,
          " of ",
          grad_fn->name(),
          " is a view and ",
          modified_obj,
          " modified inplace.");
    } else if (creation_meta == CreationMeta::INFERENCE_MODE) {
      msg = c10::str(
          "A view was created in inference mode and ",
          modified_obj,
          " modified inplace in normal mode.");
    } else {
      msg = c10::str(
          "A view was created in no_grad mode and ",
          modified_obj,
          " modified inplace with grad mode enabled.");
    }

    if (creation_meta == CreationMeta::MULTI_OUTPUT_NODE) {
      msg = c10::str(
          msg,
          " This view is the output of a function that returns multiple views. Such functions do not"
          " allow the output views to be modified inplace. You should replace the inplace operation by an"
          " out-of-place one.");
    } else if (creation_meta == CreationMeta::NO_GRAD_MODE) {
      TORCH_INTERNAL_ASSERT(!grad_fn);
      msg = c10::str(
          msg,
          " Given that this use case is ambiguous and error-prone, it is forbidden."
          " You can clarify your code by moving both the view and the inplace either both"
          " inside the no_grad block (if you don't want the inplace to be tracked) or both outside (if you want"
          " the inplace to be tracked).");
    } else if (creation_meta == CreationMeta::INFERENCE_MODE) {
      TORCH_INTERNAL_ASSERT(!grad_fn);
      msg = c10::str(
          msg,
          " Given that this use case is ambiguous and error-prone, it is forbidden."
          " You can clarify your code by moving both the view and the inplace either both"
          " inside the inference_mode block (if you don't want the inplace to be tracked) or both outside (if you want"
          " the inplace to be tracked).");
      TORCH_CHECK(false, msg);
    } else if (creation_meta == CreationMeta::IN_CUSTOM_FUNCTION) {
      msg = c10::str(
          msg,
          " This view was created inside a custom Function (or because an input was returned as-is) and the"
          " autograd logic to handle view+inplace would override the custom backward associated with the custom"
          " Function, leading to incorrect gradients. This behavior is forbidden. You can fix this by"
          " cloning the output of the custom Function.");
    } else {
      TORCH_INTERNAL_ASSERT(false, "Invalid CreationMeta state");
    }

    TORCH_CHECK(false, msg);
  }
}

} // namespace autograd
} // namespace torch
