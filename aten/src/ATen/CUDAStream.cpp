#include "ATen/CUDAStream.h"
#include "ATen/Error.h"
#include "ATen/detail/CUDAHooksInterface.h"

#include <mutex>

namespace at {

  // Internal implementation is entirely hidden 
  struct CUDAStreamInternals {
    bool is_destructible;
    std::atomic<int> refcount;
    int32_t device; // Note: cudaGetDevice works with int32_t, not int64_t
    cudaStream_t stream;
  };

  /*
  * Stream state
  */
  static constexpr cudaStream_t DEFAULT_STREAM = 0;

  static std::once_flag init_flag;
  static int64_t num_gpus;
  static CUDAStreamInternals* default_streams;
  static thread_local CUDAStreamInternals** current_streams = nullptr;

  // Creates a(n indestructible) default stream for each device
  // Note: the default stream on each device is signified by a zero
  // value for the pointer, and so is not actually created as usual.
  // In particular, we don't need to switch devices when creating the 
  // streams.
  void initDefaultCUDAStreams() {
    num_gpus = detail::getCUDAHooks().getNumGPUs();
    default_streams = (CUDAStreamInternals*) malloc(num_gpus * sizeof(CUDAStreamInternals));
    for (auto i = decltype(num_gpus){0}; i < num_gpus; ++i) {
      default_streams[i].is_destructible = false;
      default_streams[i].refcount = 0;
      default_streams[i].device = i;
      default_streams[i].stream = DEFAULT_STREAM;
    }
  }

  // Init front-end to ensure initialization only occurs once
  void initCUDAStreamsOnce() {
    // Inits default streams (once, globally)
    std::call_once(init_flag, initDefaultCUDAStreams);
    
    // Inits current streams (thread local) to default streams    
    if (current_streams) return;
    current_streams = (CUDAStreamInternals**) malloc(num_gpus * sizeof(CUDAStreamInternals*));
    for (auto i = decltype(num_gpus){0}; i < num_gpus; ++i) {
      current_streams[i] = &default_streams[i];
    }
  }

  /*
  * Internal Stream API
  */

  // Helper to return the current device
  inline int32_t current_device() {
    int cur_device;
    detail::DynamicCUDAInterface::get_device(&cur_device);
    return cur_device;
  }

  // Helper to verify the GPU index is valid
  inline void check_gpu(int64_t device) {
    AT_CHECK(device >= 0 && device < num_gpus);
  }

  CUDAStreamInternals* CUDAStream_getDefaultStreamOnDevice(int64_t device) {
    initCUDAStreamsOnce();
    check_gpu(device);
    return &default_streams[device];
  }
  CUDAStreamInternals* CUDAStream_getDefaultStream() {
    return CUDAStream_getDefaultStreamOnDevice(current_device());
  }
  
  // Creates (and retains) and new cuda stream
  CUDAStreamInternals* CUDAStream_createAndRetainWithOptions(int32_t flags, int32_t priority) {
    CUDAStreamInternals* internals = (CUDAStreamInternals*) malloc(sizeof(CUDAStreamInternals));
    internals->is_destructible = true;
    internals->refcount = 1;
    detail::DynamicCUDAInterface::get_device(&internals->device);
    detail::DynamicCUDAInterface::cuda_stream_create_with_priority(&internals->stream, flags, priority);
    return internals;
  }

  // Note: despite not being "unsafe," is using these methods in a multithreaded
  // environment then the caller must be sure that streams are valid 
  // when they're requested. These methods will throw an error if an
  // invalid stream is requested.
  CUDAStreamInternals* CUDAStream_getAndRetainCurrentStreamOnDevice(int64_t device) {
    initCUDAStreamsOnce();
    check_gpu(device);
    auto* cur = current_streams[device];
    AT_CHECK(CUDAStream_retain(cur));
    return cur;
  }
  CUDAStreamInternals* CUDAStream_getAndRetainCurrentStream() {
    return CUDAStream_getAndRetainCurrentStreamOnDevice(current_device());
  }

  // Note: these unsafe methods do not retain the stream before returning it.
  // This is unsafe behavior and these methods SHOULD NOT BE USED.
  // They are here only for legacy compatibility.
  CUDAStreamInternals* CUDAStream_getCurrentStreamOnDeviceUnsafe(int64_t device) {
    initCUDAStreamsOnce();
    check_gpu(device);
    return current_streams[device];
  }
  CUDAStreamInternals* CUDAStream_getCurrentStreamUnsafe() {
    return CUDAStream_getCurrentStreamOnDeviceUnsafe(current_device());
  }

  void CUDAStream_setStreamOnDevice(int64_t device, CUDAStreamInternals* internals) {
    initCUDAStreamsOnce();
    check_gpu(device);
    AT_CHECK(internals);
    AT_CHECK(internals->device == device);
    AT_CHECK(CUDAStream_retain(internals));

    CUDAStream_free(current_streams[device]);
    current_streams[device] = internals;
  }
  void CUDAStream_setStream(CUDAStreamInternals* internals) {
    CUDAStream_setStreamOnDevice(current_device(), internals);
  }

  // Getters
  cudaStream_t CUDAStream_stream(CUDAStreamInternals* ptr) {
    AT_CHECK(ptr);
    return ptr->stream;
  }

  int CUDAStream_device(CUDAStreamInternals* ptr) {
    AT_CHECK(ptr);
    return ptr->device;
  }

  // Memory management
  // Note: only destructible (non-default) streams are ref counted
  bool CUDAStream_retain(CUDAStreamInternals* ptr) {
    AT_CHECK(ptr);
    if (ptr->is_destructible) return(++ptr->refcount > 1);
    return true;
  }

  void CUDAStream_free(CUDAStreamInternals* ptr) {
    if (ptr && ptr->stream && ptr->is_destructible && --ptr->refcount <= 0) {
      AT_CHECK(ptr->refcount == 0);
      detail::DynamicCUDAInterface::cuda_stream_destroy(ptr->stream);
      free(ptr);
      ptr = nullptr;
    }
  }

  /*
  * CUDAStream RAII
  */

  // Copy constructor and copy-assignment operator
  void CUDAStream::copyInternal(const CUDAStream& other) {
    AT_CHECK(CUDAStream_retain(other.internals_));
    internals_ = other.internals_;
  }
  CUDAStream::CUDAStream(const CUDAStream& other) { copyInternal(other); }
  CUDAStream& CUDAStream::operator=(const CUDAStream& other) {
    copyInternal(other);
    return *this;
  }

  // Move constructor and move-assignment operator
  void CUDAStream::moveInternal(CUDAStream&& other) {
    AT_CHECK(other.internals_);
    std::swap(internals_, other.internals_);
  }
  CUDAStream::CUDAStream(CUDAStream&& other) { moveInternal(std::move(other)); }
  CUDAStream& CUDAStream::operator=(CUDAStream&& other) {
    moveInternal(std::move(other));
    return *this;
  }

} // namespace at
