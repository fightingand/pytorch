#include <torch/csrc/jit/backends/backend_resolver.h>
#include <torch/csrc/jit/frontend/sugared_value.h>
#include <torch/custom_class.h>

namespace torch::jit {
namespace {
// Essentially ClassNamespaceValue from import_source.cpp without the
// SourceImporterImpl reference. This helps resolve the
// __torch__.torch.classes.backends.{backend_name} symbols in the generated code
// for the LoweredModule.
struct ClassNamespaceValue : public SugaredValue {
  explicit ClassNamespaceValue(c10::QualifiedName name)
      : basename_(std::move(name)) {}

  std::shared_ptr<SugaredValue> attr(
      const SourceRange& loc,
      GraphFunction& m,
      const std::string& name) override {
    auto fullName = c10::QualifiedName(basename_, name);

    // Check to see if it is a custom class.
    if (auto custom_class = getCustomClass(fullName.qualifiedName())) {
      return std::make_shared<ClassValue>(custom_class);
    }

    // If it's not a custom class, assume it's another namespace
    return std::make_shared<ClassNamespaceValue>(std::move(fullName));
  }

  std::string kind() const override {
    return "Class Namespace";
  }

 private:
  c10::QualifiedName basename_;
};

// A resolver just for resolving custom backend class lookups in the
// LoweredModule classes generated by the rest of the cdoe in this file.
struct LoweredModuleResolver : public Resolver {
  std::shared_ptr<SugaredValue> resolveValue(
      const std::string& name,
      GraphFunction& m,
      const SourceRange& loc) override {
    if (name == "torch") {
      return std::make_shared<BuiltinModule>("aten");
    } else if (name == "__torch__") {
      return std::make_shared<ClassNamespaceValue>(c10::QualifiedName(name));
    } else if (name == "Exception") {
      return std::make_shared<ExceptionValue>(name);
    }

    return nullptr;
  }

  TypePtr resolveType(const std::string& name, const SourceRange& loc)
      override {
    return nullptr;
  }
};
} // namespace

std::shared_ptr<Resolver> loweredModuleResolver() {
  std::shared_ptr<Resolver> resolver =
      std::make_shared<LoweredModuleResolver>();
  return resolver;
}

} // namespace torch::jit
