#define TORCH_ASSERT_NO_OPERATORS
#include <ATen/AccumulateType.h>
#include <ATen/Dispatch.h>
#include <ATen/Dispatch_v2.h>
#include <ATen/OpMathType.h>
#include <ATen/native/DispatchStub.h>
#include <ATen/native/TensorIterator.h>
#include <ATen/native/UnaryOps.h>
#include <ATen/native/cuda/JitLoops.cuh>
#include <ATen/native/cuda/Loops.cuh>
#include <ATen/native/cuda/Math.cuh>
#include <limits>

namespace at::native {

#if AT_USE_JITERATOR()
CONSTEXPR_EXCEPT_WIN_CUDA char sin_name[] = "sin_impl";
#endif

void sin_kernel_cuda(TensorIteratorBase& iter) {
  auto common_dtype = iter.common_dtype();
  if (at::isComplexType(common_dtype)) {
#if AT_USE_JITERATOR()
    static const auto sin_string = jiterator_stringify(
        template <typename T> T sin_impl(T a) { return std::sin(a); });
    AT_DISPATCH_V2(
        common_dtype,
        "sin_name",
        AT_WRAP([&]() {
          jitted_gpu_kernel<
              /*name=*/sin_name,
              /*return_dtype=*/scalar_t,
              /*common_dtype=*/scalar_t,
              /*arity=*/1>(iter, sin_string);
        }),
        AT_EXPAND(AT_COMPLEX_TYPES),
        kComplexHalf,
    );
#else
    AT_DISPATCH_V2(
        common_dtype,
        "sin_name",
        AT_WRAP([&]() {
          gpu_kernel(iter, [] GPU_LAMBDA(scalar_t a) -> scalar_t {
            using opmath_t = at::opmath_type<scalar_t>;
            return ::sin(static_cast<opmath_t>(a));
          });
        }),
        AT_EXPAND(AT_COMPLEX_TYPES),
        kComplexHalf,
    );
#endif
  } else {
    AT_DISPATCH_V2(
        common_dtype,
        "sin_cuda",
        AT_WRAP([&]() {
          gpu_kernel(
              iter, [] GPU_LAMBDA(scalar_t a) -> scalar_t { return ::sin(a); });
        }),
        AT_EXPAND(AT_FLOATING_TYPES),
        kHalf,
        kBFloat16,
    );
  }
}

REGISTER_DISPATCH(sin_stub, &sin_kernel_cuda);

} // namespace at::native
