#include <torch/csrc/jit/fuser/kernel_cache.h>

#include <unordered_map>
#include <mutex>
#include <cstdint>

namespace torch { namespace jit { namespace fuser {

struct KernelCacheImpl {
  // Note: std::unordered_map does not invalidate references even if rehashing
  // occurs. This is a critical property for thread-safety. 
  std::mutex mutex_;
  int64_t kernel_counter{0};
  std::unordered_map<int64_t, KernelSpec> specMap_;
};

static KernelCacheImpl& getKernelCache() {
  static KernelCacheImpl cache;
  return cache;
}

int64_t debugNumCachedKernelSpecs() {
  auto& cache = getKernelCache();
  std::lock_guard<std::mutex> guard{cache.mutex_};
  return cache.specMap_.size();
}

// TODO: lookup by historic string key to start, then issue key
// as appropriate for faster lookup in the future
int64_t store(std::shared_ptr<Graph> graph) {
  auto& cache = getKernelCache();
  std::lock_guard<std::mutex> guard{cache.mutex_};
  const auto key = cache.kernel_counter++;
  cache.specMap_.emplace(
    std::piecewise_construct
  , std::forward_as_tuple(key)
  , std::forward_as_tuple(key, graph));
  return key;
}

at::optional<KernelSpec*> retrieve(const int64_t key) { 
  auto& cache = getKernelCache();
  std::lock_guard<std::mutex> guard{cache.mutex_};
  auto it = cache.specMap_.find(key);
  if (it == cache.specMap_.end()) return nullptr;
  return &(it->second);
}

// XXX: This is O(n) where n = # of key-value pairs in the kernel cache.
// Maybe we should make this average O(1) by adding a graph-to-key cache.
at::optional<KernelSpec*> lookupGraph(std::shared_ptr<Graph> graph) {
  auto& cache = getKernelCache();
  std::lock_guard<std::mutex> guard{cache.mutex_};
  std::string rep = graph->toString();
  auto it = std::find_if(std::begin(cache.specMap_), std::end(cache.specMap_),
      [&rep](const std::pair<const int64_t,KernelSpec>& kv) {
        return kv.second.graph()->toString() == rep;
      });
  if (it == cache.specMap_.end()) return at::nullopt;
  return &(it->second);
}

} // namespace fuser
} // namespace jit
} // namespace torch
