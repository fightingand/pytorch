#include <torch/csrc/jit/script/sugared_value.h>
#include <torch/csrc/jit/ir.h>
#include <torch/csrc/jit/script/schema_matching.h>
#include <torch/csrc/jit/script/tree_views.h>

namespace torch {
namespace jit {
namespace script {

struct NoneValue : SugaredValue {
  NoneValue() = default;
  std::string kind() const override {
    return "None";
  }
};

std::shared_ptr<SugaredValue> PrintValue::call(
    const SourceRange& loc,
    Function& m,
    at::ArrayRef<NamedValue> inputs,
    at::ArrayRef<NamedValue> attributes,
    size_t n_binders) {
  auto& g = *m.graph();
  if (!attributes.empty())
    throw ErrorReport(loc) << "print doesn't accept any keyword arguments";

  std::vector<Value*> lowered_inputs = toValues(*m.graph(), inputs);
  g.insertNode(g.create(prim::Print, lowered_inputs, 0)->setSourceRange(loc));
  return std::make_shared<NoneValue>();
}

static const std::unordered_map<std::string, std::string>&
builtin_cast_methods() {
  static std::unordered_map<std::string, std::string> builtin_cast_methods = {
      {"byte", "_cast_Byte"},
      {"char", "_cast_Char"},
      {"double", "_cast_Double"},
      {"float", "_cast_Float"},
      {"int", "_cast_Int"},
      {"long", "_cast_Long"},
      {"short", "_cast_Short"},
      {"half", "_cast_Half"}};
  return builtin_cast_methods;
}

std::shared_ptr<SugaredValue> BuiltinFunction::call(
    const SourceRange& loc,
    Function& m,
    at::ArrayRef<NamedValue> inputs,
    at::ArrayRef<NamedValue> attributes,
    size_t n_binders) {
  return std::make_shared<SimpleValue>(
      emitBuiltinCall(loc, *m.graph(), symbol, self, inputs, attributes, true));
}

// support syntax sugar for x.foo(y, z) by allowing x.foo to return a
// callable value that will resolve to foo(x, y, z) when called.
std::shared_ptr<SugaredValue> SimpleValue::attr(
    const SourceRange& loc,
    Function& m,
    const std::string& field) {
  // Allow method-style casts on Tensor types. e.g. x.int()
  if (value_->type()->isSubtypeOf(TensorType::get())) {
    if (builtin_cast_methods().count(field)) {
      return std::make_shared<BuiltinFunction>(
          Symbol::aten(builtin_cast_methods().at(field)),
          NamedValue(loc, "self", value_));
    }
    // functions that are just direct property lookups on tensor
    // must be registered as prim::<name>(Tensor t) -> <return_type>
    static const std::unordered_set<std::string> fields = {
        "dtype",
        "device",
        "grad",
        "data",
        "shape",
        "is_cuda",
        "is_sparse",
        "is_mkldnn",
        "is_quantized",
        "requires_grad",
        "layout",
    };
    if (fields.count(field)) {
      auto r =
          m.graph()->insert(Symbol::fromQualString("prim::" + field), {value_});
      return std::make_shared<SimpleValue>(r);
    }
  }

  // accessing fields of named tuples
  if (auto tuple_type = value_->type()->cast<TupleType>()) {
    if (tuple_type->schema()) {
      auto attrs = tuple_type->schema()->arguments();
      for (size_t i = 0; i < attrs.size(); i++) {
        if (attrs[i].name() == field) {
          auto idx = m.graph()->insertConstant(IValue(static_cast<int64_t>(i)));
          auto out_type = tuple_type->elements().at(i);
          auto r = m.graph()
                       ->insertNode(
                           m.graph()->createTupleIndex(value_, idx, out_type))
                       ->output();
          return std::make_shared<SimpleValue>(r);
        }
      }
    }
  } else if (auto classType = value_->type()->cast<ClassType>()) {
    // This is a class, emit the proper attribute lookup
    if (auto method = classType->getMethod(field)) {
      return std::make_shared<MethodValue>(getValue(), field);
    }
    if (classType->hasAttribute(field)) {
      auto& g = *m.graph();
      auto n = g.insertNode(g.createGetAttr(value_, field));
      return std::make_shared<SimpleValue>(n->output());
    }
  } else if (auto iface = value_->type()->cast<InterfaceType>()) {
    // accessing methods of interfaces
    if (auto schema = iface->getMethod(field)) {
      return std::make_shared<MethodValue>(getValue(), field);
    }
  }

  // none of the more-specific cases worked, so see if this is a builtin method
  if (auto builtin = BuiltinFunction::tryCreate(
          Symbol::aten(field), NamedValue(loc, "self", value_))) {
    return builtin;
  }

  ErrorReport report(loc);
  report << "Tried to access nonexistent attribute or method '" << field
         << "' of type '" << value_->type()->python_str() << "'.";
  if (value_->type()->kind() == ClassType::Kind) {
    report << " Did you forget to initialize an attribute in __init__()?";
  }
  throw report;
}

std::vector<std::shared_ptr<SugaredValue>> SimpleValue::asTuple(
    const SourceRange& loc,
    Function& m,
    const c10::optional<size_t>& size_hint) {
  static const auto make_simple_value =
      [](Value* v) -> std::shared_ptr<SugaredValue> {
    return std::make_shared<SimpleValue>(v);
  };
  if (value_->type()->kind() == TypeKind::TupleType) {
    auto outputs = createTupleUnpack(value_);
    return fmap(outputs, make_simple_value);
  } else if (value_->type()->kind() == TypeKind::ListType) {
    if (!size_hint) {
      throw ErrorReport(loc)
          << "cannot statically infer the expected size of a "
          << "list in this context";
    }
    auto graph = value_->owningGraph();
    Node* unpack =
        graph->insertNode(graph->createListUnpack(value_, *size_hint));
    return fmap(unpack->outputs(), make_simple_value);
  }
  throw ErrorReport(loc) << value_->type()->python_str()
                         << " cannot be used as a tuple";
}

static bool isRecursive(const TypePtr& classType, const TypePtr& attrType) {
  if (attrType->isSubtypeOf(classType)) {
    return true;
  }

  // Recursively check contained types. We need to do this because a user may do
  // A -> B -> A.
  for (const auto& type : attrType->containedTypes()) {
    if (isRecursive(classType, type)) {
      return true;
    }
  }
  return false;
}

void SimpleValue::setAttr(
    const SourceRange& loc,
    Function& m,
    const std::string& field,
    Value* newValue) {
  const auto classType = value_->type()->cast<ClassType>();
  if (!classType) {
    throw ErrorReport(loc) << "Tried to set an attribute: " << field
                           << " on a non-class: "
                           << value_->type()->python_str();
  }
  auto expectedType = classType->getAttribute(field);
  if (!expectedType) {
    // If we are still compiling the __init__ method for this class, then
    // setting an unknown attribute adds it to the class's definition.

    // We are initializing if:
    const auto isInitializing =
        // 1. The method we're currently inserting into is an init method
        // TODO this can be a qualified name check
        m.name() == "__init__" &&
        // 2. The `self` arg matches this value's type (i.e. we are in the init
        // method for this class, not some other class)
        !m.graph()->inputs().empty() &&
        m.graph()->inputs().at(0)->type() == classType;

    if (isInitializing) {
      if (isRecursive(classType, newValue->type())) {
        throw ErrorReport(loc)
            << "Assignment to attribute '" << field
            << "' cannot be of a type that contains class "
            << "'" << classType->python_str() << "'.\n"
            << "Classes that recursively contain instances of themselves"
            << " are not yet supported";
      }
      classType->addAttribute(field, newValue->type());
      expectedType = newValue->type();

      const auto insertPoint = m.graph()->insertPoint();
      const auto topLevelBlock = m.graph()->block();
      if (insertPoint->owningBlock() != topLevelBlock) {
        throw ErrorReport(loc)
            << "First assignment cannot be in a control-flow block. "
            << "Initialize the field at the top level first";
      }
    } else {
      throw ErrorReport(loc)
          << "Tried to set nonexistent attribute: " << field
          << ". Did you forget to initialize it in __init__()?";
    }
  }

  AT_ASSERT(expectedType);

  // Check type correctness
  const auto newType = newValue->type();
  if (!newType->isSubtypeOf(expectedType)) {
    throw ErrorReport(loc) << "Wrong type for attribute assignment. Expected "
                           << expectedType->python_str() << " but got "
                           << newType->python_str();
  }

  auto& g = *m.graph();
  g.insertNode(g.createSetAttr(value_, field, newValue));
}

std::shared_ptr<SugaredValue> SimpleValue::call(
    const SourceRange& loc,
    Function& m,
    at::ArrayRef<NamedValue> inputs,
    at::ArrayRef<NamedValue> attributes,
    size_t n_binders) {
  // allow our 'fake' closures to be called, used for fork serialization
  // at the moment, but can be expanded later
  Node* self = getValue()->node();
  if (self->kind() == prim::TupleConstruct && self->inputs().size() == 2 &&
      self->inputs().at(0)->node()->kind() == prim::Function) {
    std::shared_ptr<Graph> graph =
        self->inputs().at(0)->node()->g(attr::Subgraph);
    Value* context = self->inputs().at(1);
    AT_ASSERT(context->node()->kind() == prim::TupleConstruct);

    // fork nodes are emitted in their own block but we do not simplify
    // tuple construction across blocks. To ensure we clean up the tuple
    // construct create another copy of the tuple construct in the fork block
    Value* close_context =
        m.graph()
            ->insertNode(m.graph()->createTuple(context->node()->inputs()))
            ->output();
    // TODO this needs to go in `m`s compilation unit
    auto cu = std::make_shared<CompilationUnit>();
    auto fn = cu->create_function(QualifiedName("anon"), graph);
    auto ret = StrongFunctionPtr(std::move(cu), fn);

    std::vector<NamedValue> ctx_inputs = {close_context};
    ctx_inputs.insert(ctx_inputs.end(), inputs.begin(), inputs.end());
    return FunctionValue(ret).call(loc, m, ctx_inputs, attributes, n_binders);
  }

  if (auto class_type = getValue()->type()->cast<ClassType>()) {
    return attr(loc, m, "__call__")
        ->call(loc, m, inputs, attributes, n_binders);
  }

  return SugaredValue::call(loc, m, inputs, attributes, n_binders);
}

Value* SimpleValue::len(const SourceRange& loc, Function& m) {
  // List, Tuple, Tensor, fill in missing information desugaring
  Value* val = getValue();
  TypePtr val_type = val->type();
  Graph& g = *m.graph();
  if (val_type->cast<ListType>() || val_type->cast<StringType>() ||
      val_type->isSubtypeOf(TensorType::get())) {
    return g.insert(aten::len, {val}, {}, loc);
  } else {
    throw ErrorReport(loc) << "'" << val_type->python_str() << "'"
                           << " object is not iterable";
  }
}

SugaredValuePtr SimpleValue::getitem(
    const SourceRange& loc,
    Function& m,
    Value* idx) {
  Value* val = getValue();
  TypePtr val_type = val->type();
  Graph& g = *m.graph();

  // if it's a List/String/Dict, emit a regular __getitem__ op
  if (val_type->cast<ListType>() || val_type->cast<StringType>()) {
    return std::make_shared<SimpleValue>(
        g.insert(aten::__getitem__, {val, idx}, {}, loc));
  } else if (auto dict_type = val_type->cast<DictType>()) {
    return std::make_shared<SimpleValue>(
        g.insert(aten::__getitem__, {val, idx}, {}, loc));
  } else if (val_type->isSubtypeOf(TensorType::get())) {
    return std::make_shared<SimpleValue>(
        g.insert(aten::select, {val, 0, idx}, {}, loc));
  } else if (auto class_type = val_type->cast<ClassType>()) {
    return attr(loc, m, "__getitem__")->call(loc, m, {idx}, {}, 1);
  } else {
    throw ErrorReport(loc) << "'" << val_type->python_str() << "'"
                           << " object is not subscriptable";
  }
}

IterableValuePtr SimpleValue::asIterable(const SourceRange& loc, Function& m) {
  auto value = getValue();
  auto type = value->type();
  // built-in iterable types
  if (type->cast<ListType>() || type->cast<StringType>() ||
      type->cast<TensorType>()) {
    return std::make_shared<IterableValue>(
        std::make_shared<SimpleValue>(value));
  }
  // dicts iterate over keys
  if (type->cast<DictType>()) {
    return std::make_shared<IterableValue>(std::make_shared<SimpleValue>(
        m.graph()->insert(aten::keys, {value}, {}, loc)));
  }
  // we allow iteration over tuples if their types can be unified
  if (auto tup = type->cast<TupleType>()) {
    auto tuple_type = unifyTypeList(tup->elements());
    if (!tuple_type) {
      throw ErrorReport(loc)
          << "Heterogenous or empty tuples cannot be iterated over. Found "
          << type->python_str();
    }
    int64_t static_len = tup->elements().size();
    auto li = m.graph()->createList(*tuple_type, createTupleUnpack(value));
    auto out = m.graph()->insertNode(li)->output();
    return std::make_shared<IterableValue>(
        std::make_shared<SimpleValue>(out), static_len);
  } else {
    throw ErrorReport(loc) << "'" << type->python_str() << "'"
                           << " object is not iterable";
  }
}

RangeValue::RangeValue(
    const SourceRange& loc,
    Function& m,
    std::vector<Value*> inputs) {
  for (size_t i = 0; i < inputs.size(); ++i) {
    auto typ = inputs[i]->type();
    if (!typ->cast<IntType>()) {
      throw ErrorReport(loc)
          << "all inputs of range must be ints, found " << typ->python_str()
          << " in argument " << c10::guts::to_string(i);
    }
  }

  Graph& g = *m.graph();
  if (inputs.size() == 0) {
    throw ErrorReport(loc) << "range expected at least 1 arguments, got 0";
  } else if (inputs.size() == 1) {
    end_ = inputs[0];
    start_ = g.insertConstant(0, loc);
    step_ = g.insertConstant(1, loc);
    // range() call only contains end, easier to calculate len() and getitem()
    has_only_end_ = true;
  } else if (inputs.size() <= 3) {
    start_ = inputs[0];
    end_ = inputs[1];
    if (inputs.size() == 3) {
      step_ = inputs[2];
    } else {
      step_ = g.insertConstant(1, loc);
    }
    has_only_end_ = false;
  } else {
    throw ErrorReport(loc) << "range expected at most 3 arguments, got "
                           << inputs.size();
  }

  // TODO: !has_only_end calculation
  if (has_only_end_ && toIValue(end_)) {
    static_len_ = toIValue(end_)->toInt();
  } else {
    static_len_ = c10::nullopt;
  }
}

IterableValuePtr RangeValue::asIterable(const SourceRange& loc, Function& m) {
  return std::make_shared<IterableValue>(shared_from_this(), static_len_);
};

Value* RangeValue::len(const SourceRange& loc, Function& m) {
  if (has_only_end_) {
    return end_;
  } else {
    Graph& g = *m.graph();
    return g.insert(aten::__range_length, {start_, end_, step_}, {}, loc);
  }
}

SugaredValuePtr RangeValue::getitem(
    const SourceRange& loc,
    Function& m,
    Value* idx) {
  if (has_only_end_) {
    return std::make_shared<SimpleValue>(idx);
  } else {
    auto& g = *m.graph();
    return std::make_shared<SimpleValue>(
        g.insert(aten::__derive_index, {idx, start_, step_}, {}, loc));
  }
}

std::vector<SugaredValuePtr> IterableTree::get_base_iterables() {
  std::vector<SugaredValuePtr> base_iters{};

  for (SugaredValuePtr sv : children_) {
    if (auto iv = std::dynamic_pointer_cast<IterableTree>(sv)) {
      std::vector<SugaredValuePtr> child_iters = iv->get_base_iterables();
      // merge child iters with the base_iters
      base_iters.insert(
          base_iters.end(),
          std::make_move_iterator(child_iters.begin()),
          std::make_move_iterator(child_iters.end()));

    } else {
      // IterableTree leaves, either SimpleValue or RangeValue
      base_iters.emplace_back(sv);
    }
  }
  return base_iters;
}

Value* IterableTree::len(const SourceRange& loc, Function& m) {
  // if it's a iterable tree, we get the base iterables that consists of
  // SimpleValue or RangeValue, and then calculate the minimum length of all the
  // base iterables to be max_trip_count_val
  Graph& g = *m.graph();
  std::vector<SugaredValuePtr> base_iters = get_base_iterables();
  std::vector<Value*> lengths;
  lengths.reserve(base_iters.size());

  for (const SugaredValuePtr& base_iter : base_iters) {
    lengths.emplace_back(base_iter->len(loc, m));
  }
  Node* list_node = g.insertNode(g.createList(IntType::get(), lengths));
  return g.insert(prim::min, {list_node->output()}, {}, loc);
}

SugaredValuePtr IterableTree::getitem(
    const SourceRange& loc,
    Function& m,
    Value* idx) {
  std::vector<SugaredValuePtr> child_items;
  for (const SugaredValuePtr& child : children_) {
    child_items.emplace_back(child->getitem(loc, m, idx));
  }
  return std::make_shared<SugaredTupleValue>(child_items, emit_unrolled_);
}

void IterableTree::addChild(
    const SourceRange& range,
    const IterableValuePtr& iter_value) {
  auto child_len = iter_value->getLen();
  auto child_unrolled = iter_value->emitUnrolled();
  if (children_.size() == 0) {
    static_len_ = child_len;
    emit_unrolled_ = child_unrolled;
  } else {
    if ((emit_unrolled_ && !child_len) || (child_unrolled && !static_len_)) {
      throw ErrorReport(range)
          << "Can not iterate over a module list with a value "
             "that does not have a statically determinable length\n";
    }
    if (child_len && static_len_) {
      // iterables run for the minimum length of all its leaves
      static_len_ = std::min(*child_len, *static_len_);
    }
    emit_unrolled_ = emit_unrolled_ || child_unrolled;
  }

  children_.push_back(iter_value->getValue());
}

std::shared_ptr<SugaredValue> MagicMethod::call(
    const SourceRange& loc,
    Function& m,
    at::ArrayRef<NamedValue> inputs,
    at::ArrayRef<NamedValue> attributes,
    size_t n_binders) {
  if (inputs.size() > 0) {
    Value* self = inputs[0].value(*m.graph());
    if (auto class_ptr = self->type()->cast<ClassType>()) {
      return SimpleValue(self)
          .attr(loc, m, desugared_name_)
          ->call(loc, m, inputs.slice(1), attributes, n_binders);
    }
  }
  return base_value_->call(loc, m, inputs, attributes, n_binders);
}

std::shared_ptr<SugaredValue> ClassValue::call(
    const SourceRange& loc,
    Function& m,
    // note: names for args will be 'argument 0', 'argument 1', etc..
    at::ArrayRef<NamedValue> inputs,
    at::ArrayRef<NamedValue> attributes,
    size_t n_binders) {
  AT_ASSERT(n_binders <= 1);

  // Generate a new object of the right type, then call `__init__` on it
  auto& g = *m.graph();
  auto self = g.insertNode(g.createObject(type_))->output();
  if (!type_->getMethod("__init__")) {
    throw ErrorReport(loc) << "Class " << type_->name()->name()
                           << " does not have an __init__ function defined";
  }

  // Call the init function
  MethodValue(self, "__init__").call(loc, m, inputs, attributes, n_binders);

  return std::make_shared<SimpleValue>(self);
}

std::shared_ptr<SugaredValue> ClassValue::attr(
    const SourceRange& loc,
    Function& m,
    const std::string& field) {
  if (field != "__new__") {
    throw ErrorReport(loc) << "Tried to lookup unknown attribute on class";
  }
  return SpecialFormValue::create(prim::CreateObject);
}

std::shared_ptr<SugaredValue> NamedTupleConstructor::call(
    const SourceRange& loc,
    Function& m,
    at::ArrayRef<NamedValue> inputs,
    at::ArrayRef<NamedValue> attributes,
    size_t n_binders) {
  auto& g = *m.graph();

  auto schema = type_->schema();
  TORCH_INTERNAL_ASSERT(schema);
  auto qualname = type_->name();
  auto matched_schema = matchSchema(*schema, loc, g, inputs, attributes);

  auto self =
      g.insertNode(
           g.createTuple(
                matched_schema.inputs, std::move(qualname), std::move(schema))
               ->setSourceRange(loc))
          ->output();
  self->setType(type_);

  return std::make_shared<SimpleValue>(self);
}

std::shared_ptr<BuiltinFunction> BuiltinFunction::tryCreate(
    Symbol symbol,
    c10::optional<NamedValue> self) {
  for (const std::shared_ptr<Operator>& op : getAllOperatorsFor(symbol)) {
    if (!self) {
      return std::make_shared<BuiltinFunction>(symbol, nullptr);
    }
    if (auto index = op->schema().argumentIndexWithName("self")) {
      std::unordered_map<std::string, TypePtr> type_env;
      TypePtr formal_type = op->schema().arguments().at(*index).type();
      const MatchTypeReturn matched =
          matchTypeVariables(formal_type, self->type(), type_env);
      if (!matched.success()) {
        continue;
      }
      const auto concrete_type = tryEvalTypeVariables(formal_type, type_env);
      if (!concrete_type || !self->type()->isSubtypeOf(concrete_type)) {
        continue;
      }
      return std::make_shared<BuiltinFunction>(symbol, self);
    }
  }
  return nullptr;
}

} // namespace script
} // namespace jit
} // namespace torch
