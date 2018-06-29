#include <ATen/Allocator.h>

namespace at {

void deleteNothing(void*) {}
SupervisorPtr nonOwningSupervisorPtr() {
  return {nullptr, &deleteNothing};
}

static void deleteInefficientStdFunctionSupervisor(void* ptr) {
  delete static_cast<InefficientStdFunctionSupervisor*>(ptr);
}

at::SupervisedPtr
makeInefficientStdFunctionSupervisedPtr(void* ptr, const std::function<void(void*)>& deleter) {
  return {ptr, SupervisorPtr{new InefficientStdFunctionSupervisor({ptr, deleter}), &deleteInefficientStdFunctionSupervisor}};
}

} // namespace at
