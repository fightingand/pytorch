#include <ATen/detail/CUDAHooksInterface.h>

#include <ATen/Generator.h>

namespace at { namespace cuda { namespace detail {

// The real implementation of CUDAHooksInterface
class CUDAHooks : public at::detail::CUDAHooksInterface {
  std::unique_ptr<THCState, void(*)(THCState*)> initCUDA() const override;
  std::unique_ptr<Generator> initCUDAGenerator(Context*) const override;
};

// Sigh, the registry doesn't support namespaces :(
using at::detail::RegistererCUDAHooksRegistry;
using at::detail::CUDAHooksRegistry;

REGISTER_CUDA_HOOKS(CUDAHooks);

}}} // at::cuda::detail
