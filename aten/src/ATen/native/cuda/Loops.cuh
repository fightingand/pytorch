
#pragma once

#include <ATen/detail/FunctionTraits.h>
#include <ATen/native/TensorIterator.h>

namespace at { namespace native {

constexpr int num_threads = C10_WARP_SIZE * 2;
constexpr int thread_work_size = 4;
constexpr int block_work_size = thread_work_size * num_threads;

// `needs_dynamic_casting` compares the types expected by iterator
// (i.e. dtypes of the operands) with the actual type of the arguments
// of func_t
template<typename func_t, int nargs=function_traits<func_t>::arity>
struct needs_dynamic_casting {
  static bool check(TensorIterator& iter) {
    using traits = function_traits<func_t>;
    if (iter.dtype(nargs) != c10::impl::CPPTypeToScalarType<typename traits::template arg<nargs - 1>::type>::value) {
      return true;
    }
    return needs_dynamic_casting<func_t, nargs - 1>::check(iter);
  }
};

template<typename func_t>
struct needs_dynamic_casting<func_t, 0> {
  static bool check(TensorIterator& iter) {
    using traits = function_traits<func_t>;
    return iter.dtype(0) != c10::impl::CPPTypeToScalarType<typename traits::result_type>::value;
  }
};

}}  // namespace at::native

// Note:
// CUDA and ROCm get diverged in this PR:
//   https://github.com/pytorch/pytorch/pull/32383
// Because for some reason trying to enable vectorized
// memory access introduce regression on ROCm.

#ifndef __HIP_PLATFORM_HCC__
#include <ATen/native/cuda/CUDALoops.cuh>
#else
#include <ATen/native/cuda/ROCmLoops.cuh>
#endif

namespace at { namespace native {

template <typename func_t>
void gpu_kernel(TensorIterator& iter, const func_t& f) {
  ASSERT_HOST_DEVICE_LAMBDA(func_t);

  for (int arg = 0; arg < iter.ntensors(); arg++) {
    TORCH_INTERNAL_ASSERT(iter.device(arg).is_cuda());
  }

  if (iter.numel() == 0) {
    return;
  }

  if (!iter.can_use_32bit_indexing()) {
    for (auto& sub_iter : iter.with_32bit_indexing()) {
      gpu_kernel(sub_iter, f);
    }
    return;
  }

  gpu_kernel_impl(iter, f);
}

template <typename func_t>
void gpu_kernel_with_scalars(TensorIterator& iter, const func_t& f) {
  ASSERT_HOST_DEVICE_LAMBDA(func_t);
  TORCH_INTERNAL_ASSERT(iter.ntensors() == 3);

  using traits = function_traits<func_t>;
  static_assert(
      traits::arity == 2,
      "gpu_kernel_with_scalars only supports two input arguments");

  if (iter.is_cpu_scalar(1)) {
    using arg1_t = typename traits::template arg<0>::type;
    using arg2_t = typename traits::template arg<1>::type;
    auto a = iter.scalar_value<arg1_t>(1);
    iter.remove_operand(1);
    gpu_kernel(iter, [=]GPU_LAMBDA(arg2_t b) {
      return f(a, b);
    });
  } else if (iter.is_cpu_scalar(2)) {
    using arg1_t = typename traits::template arg<0>::type;
    using arg2_t = typename traits::template arg<1>::type;
    auto b = iter.scalar_value<arg2_t>(2);
    iter.remove_operand(2);
    gpu_kernel(iter, [=]GPU_LAMBDA(arg1_t a) {
      return f(a, b);
    });
  } else {
    gpu_kernel(iter, f);
  }
}

template <typename func_t>
void gpu_kernel_with_index_impl(TensorIterator& iter, const func_t& f) {
  using traits = function_traits<func_t>;
  using arg0_t = typename traits::result_type;


  // Note:
  // `gpu_kernel_with_index` was originally implemented in PR #28175 with support
  // of having an arbitrary number of tensors as arguments. This support was removed
  // during the process of refactoring Loops.cuh to support vectorized memory access
  // in PR #32777 (See also issue #31975). The removal of this support is soly because
  // at that time, there is no operator using that functionality. If you need this
  // functionality, feel free to add it back.
  static_assert(traits::arity == 1, "Functor for gpu_kernel_with_index can only have one argument which is the index");

  TORCH_INTERNAL_ASSERT(iter.ntensors() == 1);

  char* data = (char*)iter.data_ptr(0);

  int64_t numel = iter.numel();
  if (iter.is_trivial_1d()) {
    int stride = iter.get_inner_strides()[0];
    legacy::launch_kernel<launch_size_1d, 1>(numel, [=]GPU_LAMBDA(int idx) {
      arg0_t* out = (arg0_t*)(data + stride * idx);
      *out = f(idx);
    });
  } else {
    auto offset_calc = legacy::make_offset_calculator<traits::arity>(iter);
    legacy::launch_kernel<launch_size_nd, launch_bound2>(numel, [=]GPU_LAMBDA(int idx) {
      auto offsets = offset_calc.get(idx);
      arg0_t* out = (arg0_t*)(data + offsets[0]);
      *out = f(idx);
    });
  }
}

template <typename func_t>
void gpu_kernel_with_index(TensorIterator& iter, const func_t& f) {
  ASSERT_HOST_DEVICE_LAMBDA(func_t);

  TORCH_INTERNAL_ASSERT(iter.device(0).is_cuda(), "gpu_kernel_with_index only support cuda tensor.");

  if (iter.numel() == 0) {
    return;
  }

  // Split will change index, thus is not supported
  // The caller should handle the split and pass in different func
  TORCH_INTERNAL_ASSERT(iter.can_use_32bit_indexing(), "gpu_kernel_with_index only support 32-bit indexing.");

  gpu_kernel_with_index_impl(iter, f);
}

}} //namespace at::native