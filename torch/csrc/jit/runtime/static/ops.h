#pragma once

#include <ATen/Utils.h>
#include <torch/csrc/jit/ir/ir.h>
#include <torch/csrc/jit/runtime/static/impl.h>

namespace torch {
namespace jit {

using SROperator = std::function<void(ProcessedNode*)>;
using SROpFunctor = SROperator (*)(Node* n);
struct SROperatorFunctor {
  virtual SROperator Generate(Node*) {
    std::function<void(ProcessedNode*)> out;
    return out;
  }
  virtual bool CanReuseInput() {
    return false;
  }
  virtual bool CanReuseOutput() {
    return false;
  }
  virtual ~SROperatorFunctor() = default;
};

C10_DECLARE_REGISTRY(SROperatorRegistry, SROperatorFunctor);

// TODO: reuse_inp reuse_out can be deprecated with further analysis
// try to avoid this API.
#define REGISTER_OPERATOR_FUNCTOR(name, id, ...)             \
  struct SROperatorFunctor_##id : public SROperatorFunctor { \
    const SROpFunctor fn = __VA_ARGS__;                      \
    SROperator Generate(Node* n) override {                  \
      return fn(n);                                          \
    }                                                        \
  };                                                         \
  C10_REGISTER_CLASS(SROperatorRegistry, name, SROperatorFunctor_##id);

#define REGISTER_VIEW_OPERATOR_FUNCTOR(name, id, ...)        \
  struct SROperatorFunctor_##id : public SROperatorFunctor { \
    const SROpFunctor fn = __VA_ARGS__;                      \
    SROperator Generate(Node* n) override {                  \
      return fn(n);                                          \
    }                                                        \
  };                                                         \
  C10_REGISTER_CLASS(SRViewOperatorRegistry, name, SROperatorFunctor_##id);

C10_DECLARE_REGISTRY(SRViewOperatorRegistry, SROperatorFunctor);

inline at::Tensor create_empty_from(const at::Tensor& t) {
  return at::detail::empty_cpu(
      {0},
      c10::typeMetaToScalarType(t.dtype()),
      t.layout(),
      t.device(),
      c10::nullopt,
      c10::nullopt);
}

inline at::Tensor create_empty(c10::ScalarType dtype) {
  return at::detail::empty_cpu(
      {0}, dtype, c10::nullopt, c10::nullopt, c10::nullopt, c10::nullopt);
}

inline at::Tensor create_empty_from(
    const at::Tensor& t,
    c10::ScalarType dtype) {
  return at::detail::empty_cpu(
      {0}, dtype, t.layout(), t.device(), c10::nullopt, c10::nullopt);
}

inline at::Tensor create_empty_from(const at::Tensor& t, c10::Layout layout) {
  return at::detail::empty_cpu(
      {0},
      c10::typeMetaToScalarType(t.dtype()),
      layout,
      t.device(),
      c10::nullopt,
      c10::nullopt);
}

inline at::Tensor create_empty_from(const at::Tensor& t, c10::Device device) {
  return at::detail::empty_cpu(
      {0},
      c10::typeMetaToScalarType(t.dtype()),
      t.layout(),
      device,
      c10::nullopt,
      c10::nullopt);
}

inline bool checkResizedDataPtr(at::Tensor& t) {
  auto const prev_data_ptr = t.data_ptr();
  t.resize_({0});
  return prev_data_ptr == t.data_ptr();
}

inline void fastResizeToZero(at::Tensor& t) {
  t.unsafeGetTensorImpl()->set_sizes_contiguous({0});
  TORCH_INTERNAL_ASSERT_DEBUG_ONLY(checkResizedDataPtr(t));
}

bool canRunOutOfPlace(Node* n);
bool canReuseInputsOutputs(Node* n);
bool canReuseInputs(Node* n);
bool canReuseOutputs(Node* n);
bool canOptimizeConstruct(Node* n);
bool isViewOp(Node* n);

std::function<void(ProcessedNode*)> getOutOfPlaceOperation(Node* n);

bool canRunNatively(Node* n);
std::function<void(ProcessedNode*)> getNativeOperation(Node* n);

} // namespace jit
} // namespace torch
