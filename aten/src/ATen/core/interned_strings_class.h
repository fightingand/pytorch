#include <cstdint>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>
#include "ATen/core/Error.h"
#include "ATen/core/interned_strings.h"
#include <cstring>

namespace torch {
namespace jit {

struct CAFFE2_API InternedStrings {
  InternedStrings();
  Symbol symbol(const std::string& s);
  std::pair<const char*, const char*> string(Symbol sym);
  Symbol ns(Symbol sym);

 private:
  // prereq - holding mutex_
  Symbol _symbol(const std::string& s);
  std::pair<const char*, const char*> customString(Symbol sym);
  std::unordered_map<std::string, Symbol> string_to_sym_;

  struct SymbolInfo {
    Symbol ns;
    std::string qual_name;
    std::string unqual_name;
  };
  std::vector<SymbolInfo> sym_to_info_;

  std::mutex mutex_;
};

} // namespace jit
} // namespace torch
