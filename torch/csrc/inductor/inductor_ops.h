#pragma once

#include <ATen/Tensor.h>

namespace torch {
namespace inductor {

TORCH_API at::Tensor _mm_plus_mm(
    const at::Tensor& a,
    const at::Tensor& b,
    const at::Tensor& c,
    const at::Tensor& d,
    at::Tensor& out);

// Similar to as_strided with the following differences
// - offset is added to the existing offset (rather than replacing it)
// - view tracking is disabled similar to unsafe_view
TORCH_API at::Tensor _reinterpret_tensor(
    const at::Tensor& self,
    at::IntArrayRef size,
    at::IntArrayRef stride,
    int64_t offset_increment = 0);

} // namespace inductor
} // namespace torch
