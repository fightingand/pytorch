#pragma once

#include <ATen/ATen.h>

namespace torch {
namespace linalg {

#ifndef DOXYGEN_SHOULD_SKIP_THIS
namespace detail {

inline Tensor det(const Tensor& self) {
  return torch::linalg_det(self);
}

inline Tensor norm(const Tensor& self, optional<Scalar> opt_ord, optional<IntArrayRef> opt_dim, bool keepdim, optional<ScalarType> opt_dtype) {
  return torch::linalg_norm(self, opt_ord, opt_dim, keepdim, opt_dtype);
}

inline Tensor norm(const Tensor& self, std::string ord, optional<IntArrayRef> opt_dim, bool keepdim, optional<ScalarType> opt_dtype) {
  return torch::linalg_norm(self, ord, opt_dim, keepdim, opt_dtype);
}

inline Tensor& norm_out(Tensor& result, const Tensor& self, optional<Scalar> opt_ord, optional<IntArrayRef> opt_dim, bool keepdim, optional<ScalarType> opt_dtype) {
  return torch::linalg_norm_out(result, self, opt_ord, opt_dim, keepdim, opt_dtype);
}

inline Tensor& norm_out(Tensor& result, const Tensor& self, std::string ord, optional<IntArrayRef> opt_dim, bool keepdim, optional<ScalarType> opt_dtype) {
  return torch::linalg_norm_out(result, self, ord, opt_dim, keepdim, opt_dtype);
}

inline Tensor tensorsolve(const Tensor& self, const Tensor& input2, optional<IntArrayRef> axes) {
  return torch::linalg_tensorsolve(self, input2, axes);
}

} // namespace detail
#endif /* DOXYGEN_SHOULD_SKIP_THIS */


/// See the documentation of torch.linalg.det
inline Tensor linalg_det(const Tensor& self) {
  return detail::det(self);
}

inline Tensor linalg_norm(const Tensor& self, optional<Scalar> opt_ord, optional<IntArrayRef> opt_dim, bool keepdim, optional<ScalarType> opt_dtype) {
  return detail::norm(self, opt_ord, opt_dim, keepdim, opt_dtype);
}

inline Tensor linalg_norm(const Tensor& self, std::string ord, optional<IntArrayRef> opt_dim, bool keepdim, optional<ScalarType> opt_dtype) {
  return detail::norm(self, ord, opt_dim, keepdim, opt_dtype);
}

inline Tensor& linalg_norm_out(Tensor& result, const Tensor& self, optional<Scalar> opt_ord, optional<IntArrayRef> opt_dim, bool keepdim, optional<ScalarType> opt_dtype) {
  return detail::norm_out(result, self, opt_ord, opt_dim, keepdim, opt_dtype);
}

inline Tensor& linalg_norm_out(Tensor& result, const Tensor& self, std::string ord, optional<IntArrayRef> opt_dim, bool keepdim, optional<ScalarType> opt_dtype) {
  return detail::norm_out(result, self, ord, opt_dim, keepdim, opt_dtype);
}

/// Solving of the tensor equation `a x = b`
///
/// See https://pytorch.org/docs/master/linalg.html#torch.linalg.tensorsolve
///
/// Example:
/// ```
/// auto a = torch::eye(2*3*4).reshape({2*3, 4, 2, 3, 4});
/// auto b = torch::randn(2*3, 4);
/// auto x = torch::linalg::tensorsolve(a, b);
/// ```
inline Tensor tensorsolve(const Tensor& a, const Tensor& b, optional<IntArrayRef> axes) {
  return detail::tensorsolve(a, b, axes);
}

}} // torch::linalg
