#include <torch/csrc/jit/jit_log.h>
#include <torch/csrc/jit/passes/onnx/function_extraction.h>

namespace torch {
namespace jit {
namespace onnx {

namespace {

using scope_list = std::vector<ScopePtr>;

static bool HasSameAttribute(const Node* a, const Node* b, const c10::Symbol& attr);

struct FunctionExtractor {
 public:
  FunctionExtractor(const std::shared_ptr<Graph>& graph,
      const std::vector<std::string>& module_names,
      const std::vector<std::string>& param_names)
    : graph_(graph), module_names_(module_names.begin(), module_names.end()), param_names_(param_names.begin(), param_names.end()) {}
  std::pair<ValAttrNameMap, NodeAttrNameMap> run();

 private:
  struct ScopeContext {
    std::unordered_set<ScopePtr> children_;
    ScopePtr scope_;
    node_list nlist_;
    value_list inputs_;
    value_list outputs_;
    std::unordered_map<Value*, Value*> env_to_subgraph_;

    void PopulateInputsOutputs(const std::unordered_set<std::string>& param_names);
    bool IsIdenticalFuncion(const ScopeContext& other_ctx) const;
  };

  using ScopeCtxPtr = ScopeContext*;
  using scope_ctx_map = std::unordered_map<ScopePtr, ScopeCtxPtr>;
  using scope_ctx_list = std::vector<ScopeContext>;

  struct FunctionContext {
    FunctionContext(ScopePtr key, const scope_list& scopes, scope_ctx_map& scope_ctxs);
    void DebugPrint() const;
    void SetAttrName(Node* ref_n, Symbol attr, const std::string& name);
    void SetAttrName(Node* ref_const_n, const std::string& name);
    c10::optional<std::string> FindAttrName(Node* ref_n, Symbol attr);
    c10::optional<std::string> FindAttrName(Node* ref_const_n);

    ScopePtr scope_key_;
    scope_ctx_map scope_ctxs_;
    std::unordered_map<Node*, std::unordered_set<Node*>> constant_map_;
    std::unordered_map<Node*, std::unordered_map<Symbol, std::unordered_set<Node*>>> attribute_map_;

    // Passed later to serialization.
    ValAttrNameMap val_attr_to_name_;
    NodeAttrNameMap node_attr_to_name_;
  };

  using FunctionCtxPtr = FunctionContext*;
  using func_ctx_map = std::unordered_map<ScopePtr, FunctionCtxPtr>;

  static bool IsValidScope(ScopePtr s);
  static c10::optional<ScopePtr> InferScope(Node* n);
  static bool IsAncester(ScopePtr parent, ScopePtr child);
  static c10::optional<ScopePtr> FindCommonAncestor(ScopePtr a, ScopePtr b);
  static c10::optional<ScopePtr> FindCommonAncestor(const scope_list& scopes);
  std::shared_ptr<Graph> ConstructFuncGraph(FunctionContext& ctx);

  void ConvertScopeToFunction(
      const ScopePtr& scope_key,
      const scope_list& scope_list,
      scope_ctx_map& scope_ctxs,
      const std::shared_ptr<Graph>& graph);

  static void HandleNoScopeNodes(scope_ctx_map&, node_list no_scope_nlist);
  std::tuple<scope_ctx_map, node_list> PartitionNodesByScope(Block* b);
  scope_ctx_map PartitionNodesByScope(const std::shared_ptr<Graph>& graph);
  static std::unordered_map<ScopePtr, scope_list> PartitionIdenticalScopes(scope_ctx_map& scope_ctxs);
  static func_ctx_map GenFuncCtx(const std::unordered_map<ScopePtr, scope_list>&, scope_ctx_map& scope_ctxs);
  static scope_list SortScopesByMaxDepth(std::unordered_map<ScopePtr, scope_list>&);
  Node* CreateFunctionDefNode(FunctionContext& func_ctx, const std::shared_ptr<Graph>& graph, const std::string& domain_name, const std::string& func_name);
  Node* CreateFunctionNode(FunctionContext& func_ctx, ScopeContext& scope_ctx, const std::shared_ptr<Graph>& graph, const std::string& domain_name, const std::string& func_name);
  static void ReplaceNodesWithFunctionNode();

  static void DebugPrintScopeContexts(const scope_ctx_map&);
  static void DebugPrintGraphWithFunction(const std::shared_ptr<Graph>& g);
  static void DebugPrintConstantDiff(const FunctionContext&);

  std::shared_ptr<Graph> graph_;
  std::unordered_set<std::string> module_names_;
  std::unordered_set<std::string> param_names_;
  func_ctx_map func_ctxs_;
};

FunctionExtractor::FunctionContext::FunctionContext(ScopePtr key, const scope_list& scopes, scope_ctx_map& scope_ctxs)
  : scope_key_(key) {
  GRAPH_UPDATE("Process function family for scope ", scope_key_->name().toDisplayString());
  TORCH_INTERNAL_ASSERT(scopes.size() > 0);
  const auto& ref_ctx = scope_ctxs[scope_key_];
  // NOTE: Family scopes must have same number and order of nodes.
  GRAPH_DEBUG("Initialized family context for scope ", scope_key_->name().toDisplayString());

  for (const auto& scope : scopes) {
    GRAPH_DEBUG("Process scope family with scope ", scope->name().toDisplayString());
    TORCH_INTERNAL_ASSERT(scope_ctxs.find(scope) != scope_ctxs.end());
    scope_ctxs_[scope] = scope_ctxs[scope];
    if (scope_key_ == scope) {
      continue;
    }
    auto& scope_ctx = scope_ctxs[scope];

    const auto& ns_a = ref_ctx->nlist_;
    const auto& ns_b = scope_ctx->nlist_;
    TORCH_INTERNAL_ASSERT(ns_a.size() == ns_b.size());

    GRAPH_DEBUG("Process nodes of scope ", scope->name().toDisplayString());
    for (const auto i : c10::irange(ns_a.size())) {
      TORCH_INTERNAL_ASSERT(ns_a[i]->kind() == ns_b[i]->kind());
      auto n_a = ns_a[i];
      auto n_b = ns_b[i];
      if (n_a->kind() == c10::onnx::Constant) {
        auto t_a = n_a->t(attr::value);
        auto t_b = n_b->t(attr::value);
        if (!t_a.equal(t_b)) {
          constant_map_[n_a].insert(n_b);
        }
      } else {
        std::vector<c10::Symbol> diff_attrs;
        std::vector<c10::Symbol> same_attrs;
        auto n_a_attr_names = n_a->attributeNames();
        auto n_b_attr_names = n_b->attributeNames();
        std::sort(n_a_attr_names.begin(), n_a_attr_names.end());
        std::sort(n_b_attr_names.begin(), n_b_attr_names.end());
        std::set_difference(n_a_attr_names.begin(), n_a_attr_names.end(), n_b_attr_names.begin(), n_b_attr_names.end(), std::inserter(diff_attrs, diff_attrs.begin()));
        std::set_intersection(n_a_attr_names.begin(), n_a_attr_names.end(), n_b_attr_names.begin(), n_b_attr_names.end(), std::inserter(same_attrs, same_attrs.begin()));
        for (auto attr_name : diff_attrs) {
          attribute_map_[n_a][attr_name].insert(n_b);
        }

        for (auto attr_name : same_attrs) {
          if (!HasSameAttribute(n_a, n_b, attr_name)) {
            attribute_map_[n_a][attr_name].insert(n_b);
          }
        }
      }
    }
    GRAPH_DEBUG("Process scope complete. ", scope->name().toDisplayString());
  }

  GRAPH_DEBUG("Process scope family complete. ", scope_key_->name().toDisplayString());
  DebugPrint();
}

void FunctionExtractor::FunctionContext::DebugPrint() const {
  GRAPH_DEBUG("Scope name: ", scope_key_->name().toDisplayString());

  for (const auto& it : constant_map_) {
    GRAPH_DEBUG("Constant value difference: ");
    GRAPH_DEBUG(*it.first);
    for (auto n : it.second) {
      GRAPH_DEBUG(*n);
    }
  }

  for (const auto& it : attribute_map_) {
    for (const auto& attr_it : it.second) {
      GRAPH_DEBUG("Attribute value difference for attribute ", attr_it.first.toDisplayString());
      GRAPH_DEBUG(*it.first);
      for (auto n : attr_it.second) {
        GRAPH_DEBUG(*n);
      }
    }
  }
}

void FunctionExtractor::FunctionContext::SetAttrName(Node* ref_n, Symbol attr, const std::string& name) {
  auto v_it = scope_ctxs_[scope_key_]->env_to_subgraph_.find(ref_n->outputs().at(0));
  TORCH_INTERNAL_ASSERT(v_it != scope_ctxs_[scope_key_]->env_to_subgraph_.end());
  auto* n_in_def = v_it->second->node();
  auto n_attr_it = node_attr_to_name_[n_in_def][attr.toUnqualString()] = name;
}

void FunctionExtractor::FunctionContext::SetAttrName(Node* ref_const_n, const std::string& name) {
  TORCH_INTERNAL_ASSERT(ref_const_n->kind() == c10::onnx::Constant);
  auto v_it = scope_ctxs_[scope_key_]->env_to_subgraph_.find(ref_const_n->output());
  TORCH_INTERNAL_ASSERT(v_it != scope_ctxs_[scope_key_]->env_to_subgraph_.end(), "node not found in env: ", *ref_const_n);
  val_attr_to_name_[v_it->second] = name;
}

c10::optional<std::string> FunctionExtractor::FunctionContext::FindAttrName(Node* ref_n, Symbol attr) {
  auto v_it = scope_ctxs_[scope_key_]->env_to_subgraph_.find(ref_n->outputs().at(0));
  if (v_it == scope_ctxs_[scope_key_]->env_to_subgraph_.end()) {
    return c10::nullopt;
  }
  auto* n_in_def = v_it->second->node();
  auto n_attr_it = node_attr_to_name_.find(n_in_def);
  if (n_attr_it == node_attr_to_name_.end()) {
    return c10::nullopt;
  }
  auto name_it = n_attr_it->second.find(attr.toUnqualString());
  if (name_it == n_attr_it->second.end()) {
    return c10::nullopt;
  }
  return name_it->second;
}

c10::optional<std::string> FunctionExtractor::FunctionContext::FindAttrName(Node* ref_const_n) {
  TORCH_INTERNAL_ASSERT(ref_const_n->kind() == c10::onnx::Constant);
  auto v_it = scope_ctxs_[scope_key_]->env_to_subgraph_.find(ref_const_n->output());
  if (v_it == scope_ctxs_[scope_key_]->env_to_subgraph_.end()) {
    return c10::nullopt;
  }
  auto name_it = val_attr_to_name_.find(v_it->second);
  if (name_it == val_attr_to_name_.end()) {
    return c10::nullopt;
  }
  return name_it->second;
}

void FunctionExtractor::DebugPrintScopeContexts(const scope_ctx_map& scope_ctxs) {
  for (auto& it : scope_ctxs) {
    GRAPH_UPDATE(
        "Scope name: ",
        it.first->namesFromRoot(),
        " ",
        it.first->name().toDisplayString());
    GRAPH_UPDATE("Children scopes: ", [&]() {
      std::stringstream ss;
      for (const auto& child_scope : it.second->children_) {
        ss << child_scope->name().toDisplayString() << " ";
      }
      return ss.str();
    }());
    GRAPH_UPDATE("Node types: \n", [&]() {
      std::stringstream ss;
      for (auto n : it.second->nlist_) {
        ss << "  " << *n;
      }
      return ss.str();
    }());
    GRAPH_UPDATE("Node count: ", it.second->nlist_.size());
  }
}

void FunctionExtractor::DebugPrintGraphWithFunction(const std::shared_ptr<Graph>& g) {
  GRAPH_UPDATE("Local function definitions:");
  for (auto* n : g->nodes()) {
    if (n->kind() == Symbol::onnx("LocalFunctionDef")) {
      GRAPH_UPDATE(n->s(attr::name), " graph: ", n->g(Symbol::attr("graph"))->toString());
    }
  }
  GRAPH_UPDATE("Main graph: ", g->toString());
}

bool FunctionExtractor::IsValidScope(ScopePtr s) {
  return !s->isRoot() && !s->isBlank();
}

bool FunctionExtractor::IsAncester(ScopePtr parent, ScopePtr child) {
  if (!IsValidScope(parent) || !IsValidScope(child) ||
      parent->getDepth() >= child->getDepth()) {
    return false;
  }
  do {
    child = child->parent();
    if (parent == child) {
      return true;
    }
  } while (IsValidScope(child));
  return false;
}

c10::optional<ScopePtr> FunctionExtractor::FindCommonAncestor(
    ScopePtr a,
    ScopePtr b) {
  auto common_ancestor = a;

  if (!IsValidScope(a) || !IsValidScope(b)) {
    return c10::nullopt;
  }

  auto diff =
      static_cast<int64_t>(a->getDepth()) - static_cast<int64_t>(b->getDepth());
  if (diff != 0) {
    auto deeper_scope = diff > 0 ? a : b;
    auto other_scope = diff > 0 ? b : a;
    while (diff > 0) {
      deeper_scope = deeper_scope->parent();
      diff--;
    }
    a = deeper_scope;
    b = other_scope;
  }

  while (IsValidScope(a) && IsValidScope(b)) {
    if (a == b) {
      return a;
    } else {
      a = a->parent();
      b = b->parent();
    }
  }

  return c10::nullopt;
}

c10::optional<ScopePtr> FunctionExtractor::FindCommonAncestor(
    const scope_list& scopes) {
  if (scopes.size() == 0) {
    return c10::nullopt;
  }

  c10::optional<ScopePtr> common_ancestor = scopes.at(0);
  for (auto scope : scopes) {
    common_ancestor = FindCommonAncestor(common_ancestor.value(), scope);
    if (!common_ancestor.has_value()) {
      return c10::nullopt;
    }
  }

  return common_ancestor;
}

c10::optional<ScopePtr> FunctionExtractor::InferScope(Node* n) {
  scope_list input_scopes;
  scope_list output_scopes;
  for (auto input : n->inputs()) {
    input_scopes.emplace_back(input->node()->scope());
  }
  for (auto output : n->outputs()) {
    for (auto use : output->uses()) {
      output_scopes.emplace_back(use.user->scope());
    }
  }
  if (output_scopes.size() > 0 &&
      std::all_of(
          output_scopes.begin(),
          output_scopes.end(),
          [&output_scopes](ScopePtr scope) -> bool {
            return IsValidScope(scope) && scope == output_scopes.at(0);
          })) {
    return output_scopes.at(0);
  } else if (
      input_scopes.size() > 0 &&
      std::all_of(
          input_scopes.begin(),
          input_scopes.end(),
          [&input_scopes](ScopePtr scope) -> bool {
            return IsValidScope(scope) && scope == input_scopes.at(0);
          })) {
    return input_scopes.at(0);
  } else {
    scope_list scopes;
    std::copy_if(
        input_scopes.begin(), input_scopes.end(), scopes.begin(), IsValidScope);
    std::copy_if(
        output_scopes.begin(),
        output_scopes.end(),
        scopes.begin(),
        IsValidScope);

    if (scopes.size() > 0) {
      // find common ancestor.
      auto common_ancestor = FindCommonAncestor(scopes);

      if (common_ancestor.has_value() &&
          IsValidScope(common_ancestor.value())) {
        return common_ancestor.value();
      }
    }
  }

  return c10::nullopt;
}

std::shared_ptr<Graph> FunctionExtractor::ConstructFuncGraph(
    FunctionContext& func_ctx) {
  auto& ctx = *func_ctx.scope_ctxs_[func_ctx.scope_key_];
  const auto& nlist = ctx.nlist_;
  const auto& scope = ctx.scope_;
  auto& env = ctx.env_to_subgraph_;

  auto g = std::make_shared<Graph>();
  GRAPH_DEBUG("Constructing graph for ", scope->namesFromRoot());

  // TODO: update input name of function to common name?
  ctx.PopulateInputsOutputs(param_names_);
  for (auto* v : ctx.inputs_) {
    env[v] = g->addInput()->copyMetadata(v);
    GRAPH_DEBUG(
        "Add input value ",
        env[v]->debugName(),
        " for outer scope value ",
        v->debugName(),
        " from ",
        *v->node());
  }

  for (auto* n : nlist) {
    auto clone_n = g->createClone(n, [&](Value* v) {
      TORCH_INTERNAL_ASSERT(env.find(v) != env.end());
      return env[v];
    });
    for (const auto i : c10::irange(clone_n->outputs().size())) {
      env[n->output(i)] = clone_n->output(i);
    }
    g->insertNode(clone_n);
  }

  // If values are used outside of this graph, set as graph output.
  for (auto* v : ctx.outputs_) {
    TORCH_INTERNAL_ASSERT(env.find(v) != env.end());
    g->registerOutput(env[v]);
  }

  GRAPH_DEBUG(g->toString());
  return g;
}

Node* FunctionExtractor::CreateFunctionDefNode(FunctionContext& func_ctx, const std::shared_ptr<Graph>& graph, const std::string& domain_name, const std::string& func_name) {
  const auto func_def_nk = Symbol::onnx("LocalFunctionDef");
  const auto func_g_attr = Symbol::attr("graph");
  const auto func_name_attr = attr::name;
  const auto func_domain_attr = Symbol::attr("domain");

  auto func_graph = ConstructFuncGraph(func_ctx);

  // create and insert local function definition node
  auto func_def_n = graph->create(func_def_nk, 0);
  func_def_n->g_(func_g_attr, func_graph);
  func_def_n->s_(func_name_attr, func_name);
  func_def_n->s_(func_domain_attr, domain_name);
  graph->prependNode(func_def_n);

  // set constants and attributes of different values as function attributes.
  std::unordered_map<std::string, int> base_attr_name_count;
  std::vector<std::string> final_attr_names;

  auto adjust_attr_name = [&](std::string attr_name) {
    if (base_attr_name_count.find(attr_name) != base_attr_name_count.end()) {
      attr_name = attr_name + "." + std::to_string(base_attr_name_count[attr_name]++);
    } else {
      base_attr_name_count[attr_name] = 1;
    }
    return attr_name;
  };

  for (const auto c_it : func_ctx.constant_map_) {
    auto* c_n = c_it.first;
    TORCH_INTERNAL_ASSERT(c_n->kind() == c10::onnx::Constant);
    auto attr_name = std::string(c_n->kind().toUnqualString()) + '_' + c_n->output()->debugNameBase();
    auto final_attr_name = adjust_attr_name(attr_name);
    final_attr_names.emplace_back(final_attr_name);
    func_ctx.SetAttrName(c_n, final_attr_name);
  }
  for (const auto n_it : func_ctx.attribute_map_) {
    auto* n = n_it.first;
    for (const auto attr_it : n_it.second) {
      const auto& attr = attr_it.first;
      auto attr_name = std::string(n->kind().toUnqualString()) + '_' + attr.toUnqualString();
      auto final_attr_name = adjust_attr_name(attr_name);
      final_attr_names.emplace_back(final_attr_name);
      func_ctx.SetAttrName(n, attr, final_attr_name);
    }
  }

  func_def_n->ss_(Symbol::attr("attributes"), final_attr_names);

  return func_def_n;
}

Node* FunctionExtractor::CreateFunctionNode(FunctionContext& func_ctx, ScopeContext& scope_ctx, const std::shared_ptr<Graph>& graph, const std::string& domain_name, const std::string& func_name) {
  const auto& func_scope = func_ctx.scope_key_;
  const auto& func_scope_ctx = func_ctx.scope_ctxs_[func_scope];
  const auto func_nk = Symbol::fromQualString(domain_name + "::" + func_name);
  GRAPH_DEBUG(
      "Create and insert local function for scope: ",
      func_scope->namesFromRoot());
  scope_ctx.PopulateInputsOutputs(param_names_);
  auto last_n = *scope_ctx.nlist_.rbegin();
  auto func_n = graph->create(func_nk, scope_ctx.outputs_.size());
  func_n->copyMetadata(last_n);
  for (auto* v : scope_ctx.inputs_) {
    func_n->addInput(v);
  }
  for (const auto i : c10::irange(scope_ctx.outputs_.size())) {
    func_n->output(i)->setType(scope_ctx.outputs_[i]->type());
    scope_ctx.outputs_[i]->replaceAllUsesWith(func_n->output(i));
  }

  // set constants and attributes of different values as function attributes.

  for (const auto& it : func_ctx.constant_map_) {
    TORCH_INTERNAL_ASSERT(it.first->kind() == c10::onnx::Constant);
    TORCH_INTERNAL_ASSERT(it.first->outputs().size() == 1);
    auto attr_name = func_ctx.FindAttrName(it.first).value();
    auto val = it.first->t(attr::value);
    for (auto* n : scope_ctx.nlist_) {
      if (it.second.find(n) != it.second.end()) {
        val = n->t(attr::value);
        break;
      }
    }
    func_n->t_(Symbol::attr(attr_name), val);
  }

  auto copy_attr = [](Node* a, Node* b, Symbol attr, const std::string& new_name) {
#define COPY_ATTR(kind)                                     \
      case AttributeKind::kind: {                           \
        b->kind##_(Symbol::attr(new_name), a->kind(attr));  \
        break;                                              \
      }
    switch (a->kindOf(attr)) {
      COPY_ATTR(f)
      COPY_ATTR(fs)
      COPY_ATTR(i)
      COPY_ATTR(is)
      COPY_ATTR(s)
      COPY_ATTR(ss)
      COPY_ATTR(t)
      COPY_ATTR(ts)
#undef COPY_ATTR
      case AttributeKind::ival:
      case AttributeKind::g:
      case AttributeKind::gs:
      case AttributeKind::ty:
      case AttributeKind::tys:
      case AttributeKind::c:
      default:
        TORCH_INTERNAL_ASSERT(false, "Unexpected attribute type ", a->kindOf(attr), " from node ", *a);
        break;
    }
  };

  for (const auto& it : func_ctx.attribute_map_) {
    auto* ref_n = it.first;
    for (const auto attr_it : it.second) {
      const auto& attr = attr_it.first;
      auto attr_name = func_ctx.FindAttrName(ref_n, attr).value();
      copy_attr(ref_n, func_n, attr, attr_name);
      for (auto* n : scope_ctx.nlist_) {
        if (attr_it.second.find(n) != attr_it.second.end()) {
          copy_attr(n, func_n, attr, attr_name);
          break;
        }
      }
    }
  }

  func_n->insertAfter(last_n);

  return func_n;
}

void FunctionExtractor::ConvertScopeToFunction(
    const ScopePtr& scope_key,
    const scope_list& scope_list,
    scope_ctx_map& scope_ctxs,
    const std::shared_ptr<Graph>& graph) {
  // This function needs to be called always on inner most scopes.
  // 1. Generate function context, this identifies different constants and attributes.
  // 2. Create function definition node, and insert to main graph.
  // 3. Create function node for each call, and replace subgraph nodes in parent functions.

  func_ctxs_.insert(std::make_pair(scope_key, new FunctionContext(scope_key, scope_list, scope_ctxs)));
  auto& func_ctx = *func_ctxs_[scope_key];

  const std::string module_class_name(func_ctx.scope_key_->name().toUnqualString());
  auto pos = module_class_name.rfind(".");
  TORCH_INTERNAL_ASSERT(pos != std::string::npos);
  const auto domain_name = module_class_name.substr(0, pos);
  const auto func_name = module_class_name.substr(pos + 1);

  auto func_def_n = CreateFunctionDefNode(func_ctx, graph, domain_name, func_name);

  // create and insert local function node to graph.
  for (const auto& it : func_ctx.scope_ctxs_) {
    auto scope = it.first;
    auto& scope_ctx = *it.second;
    auto func_n = CreateFunctionNode(func_ctx, scope_ctx, graph, domain_name, func_name);

    std::unordered_set<Node*> old_nodes(
        scope_ctx.nlist_.begin(), scope_ctx.nlist_.end());

    auto last_n = *scope_ctx.nlist_.rbegin();
    // replace local function node in parent scopes.
    for (auto& it : scope_ctxs) {
      const auto& parent_scope = it.first;
      auto& parent_ctx = *it.second;

      if (!IsAncester(parent_scope, scope)) {
        continue;
      }

      auto& ctx_nlist = parent_ctx.nlist_;
      GRAPH_DEBUG(
          "Replace local function node in parent scope: ",
          it.first->namesFromRoot(),
          " nodes to remove: ",
          old_nodes.size(),
          " parent total nodes: ",
          ctx_nlist.size());

      // insert local function node
      auto last_n_it = std::find(ctx_nlist.begin(), ctx_nlist.end(), last_n);
      ctx_nlist.insert(last_n_it, func_n);

      // remove replaced nodes from list
      ctx_nlist.erase(
          std::remove_if(
              ctx_nlist.begin(),
              ctx_nlist.end(),
              [&old_nodes](Node* n) {
                return old_nodes.find(n) != old_nodes.end();
              }),
          ctx_nlist.end());

      GRAPH_DEBUG("Parent total nodes after remove: ", ctx_nlist.size());

      // refresh inputs/outputs.
      parent_ctx.PopulateInputsOutputs(param_names_);
    }
  }

  for (const auto& it : func_ctx.scope_ctxs_) {
    auto& scope_ctx = *it.second;
    // delete replaced nodes in graph.
    for (auto it = scope_ctx.nlist_.rbegin(); it != scope_ctx.nlist_.rend();) {
      auto* n = *it;
      it++;
      GRAPH_DEBUG("Destroying node ", *n);
      n->destroy();
    }
  }
}

bool FunctionExtractor::ScopeContext::IsIdenticalFuncion(
    const ScopeContext& other_ctx) const {
  // TODO: need to differentiate same function under different inputs.
  //       when constants are passed inplace of inputs, this leads to different
  //       input count and node count. Likewise, due to different uses, output
  //       count can be different as well.
  if (&other_ctx == this) {
    return true;
  }
  if (this->scope_->name() != other_ctx.scope_->name()) {
    return false;
  }
  if (this->inputs_.size() != other_ctx.inputs_.size() ||
      this->outputs_.size() != other_ctx.outputs_.size()) {
    return false;
  }
  const auto& ns_a = this->nlist_;
  const auto& ns_b = other_ctx.nlist_;
  if (ns_a.size() != ns_b.size()) {
    return false;
  }
  for (const auto i : c10::irange(ns_a.size())) {
    if (ns_a[i]->kind() != ns_b[i]->kind()) {
      return false;
    }
  }

  return true;
}

void FunctionExtractor::ScopeContext::PopulateInputsOutputs(const std::unordered_set<std::string>& param_names) {
  inputs_.clear();
  outputs_.clear();
  const auto& nlist = this->nlist_;
  std::unordered_set<Value*> v_set;
  std::unordered_set<Node*> n_set;

  value_list input_list;
  value_list initializer_list;

  // Add initializers after inputs.
  for (auto* n : nlist) {
    for (auto* v : n->inputs()) {
      if (v_set.find(v) == v_set.end()) {
        if (param_names.find(v->debugName()) != param_names.end()) {
          initializer_list.emplace_back(v);
        } else {
          input_list.emplace_back(v);
        }
        v_set.insert(v);
      }
    }
    for (auto* v : n->outputs()) {
      v_set.insert(v);
    }
    n_set.insert(n);
  }
  for (auto* v : input_list) {
    inputs_.emplace_back(v);
  }
  for (auto* v : initializer_list) {
    inputs_.emplace_back(v);
  }

  for (auto* n : nlist) {
    for (auto* v : n->outputs()) {
      bool used_outside = false;
      for (auto use : v->uses()) {
        used_outside |= (n_set.find(use.user) == n_set.end());
      }
      if (used_outside) {
        outputs_.emplace_back(v);
      }
    }
  }
}

void FunctionExtractor::HandleNoScopeNodes(
    scope_ctx_map& scope_ctxs,
    node_list no_scope_nlist) {
  GRAPH_UPDATE("No scope node count: ", no_scope_nlist.size());
  for (auto n : no_scope_nlist) {
    TORCH_WARN("ONNX function extraction cannot determine the scope for node: ", *n);
  }
  TORCH_INTERNAL_ASSERT(false, "ONNX function extraction cannot determine the scope for the above nodes.");
}

std::tuple<FunctionExtractor::scope_ctx_map, node_list> FunctionExtractor::
    PartitionNodesByScope(Block* b) {
  scope_ctx_map scope_ctxs = {};
  node_list no_scope_nlist;

  auto find_or_create_scope_ctx = [](scope_ctx_map& scope_ctxs, const ScopePtr& scope) {
    if (scope_ctxs.find(scope) == scope_ctxs.end()) {
      scope_ctxs.insert(std::make_pair(scope, new ScopeContext()));
    }
    return scope_ctxs[scope];
  };

  auto record_node_scope = [&scope_ctxs, &find_or_create_scope_ctx](Node* n) {
    const auto& scope = n->scope();
    find_or_create_scope_ctx(scope_ctxs, scope)->scope_ = scope;
    auto tmp_scope = scope;
    while (IsValidScope(tmp_scope)) {
      find_or_create_scope_ctx(scope_ctxs, tmp_scope)->nlist_.emplace_back(n);
      if (IsValidScope(tmp_scope->parent())) {
        find_or_create_scope_ctx(scope_ctxs, tmp_scope->parent())->children_.insert(tmp_scope);
      }
      tmp_scope = tmp_scope->parent();
    }
  };

  for (auto* n : b->nodes()) {
    auto scope = n->scope();
    if (scope && IsValidScope(scope)) {
      record_node_scope(n);
    } else {
      auto inferred_scope = InferScope(n);

      if (inferred_scope.has_value() && IsValidScope(inferred_scope.value())) {
        n->setScope(inferred_scope.value());
        record_node_scope(n);
      } else {
        GRAPH_UPDATE("Cannot infer proper scope for node: ", *n);
        no_scope_nlist.emplace_back(n);
      }
    }

    for (auto* sub_b : n->blocks()) {
      scope_ctx_map subblock_scope_ctxs;
      node_list subblock_no_scope_nlist;
      std::tie(subblock_scope_ctxs, subblock_no_scope_nlist) =
          PartitionNodesByScope(sub_b);

      for (auto& it : subblock_scope_ctxs) {
        if (scope_ctxs.find(it.first) == scope_ctxs.end()) {
          scope_ctxs.insert(std::make_pair(it.first, it.second));
        } else {
          for (auto* s_n : it.second->nlist_) {
            scope_ctxs[it.first]->nlist_.emplace_back(s_n);
          }
          for (auto s_child_scope : it.second->children_) {
            scope_ctxs[it.first]->children_.insert(s_child_scope);
          }
        }
      }

      no_scope_nlist.insert(
          no_scope_nlist.end(),
          subblock_no_scope_nlist.begin(),
          subblock_no_scope_nlist.end());
    }
  }

  for (auto& it : scope_ctxs) {
    it.second->scope_ = it.first;
    it.second->PopulateInputsOutputs(param_names_);
  }

  return std::tie(scope_ctxs, no_scope_nlist);
}

FunctionExtractor::scope_ctx_map FunctionExtractor::PartitionNodesByScope(
    const std::shared_ptr<Graph>& graph) {
  scope_ctx_map scope_ctxs;
  node_list no_scope_nlist;
  std::tie(scope_ctxs, no_scope_nlist) = PartitionNodesByScope(graph->block());

  HandleNoScopeNodes(scope_ctxs, no_scope_nlist);

  return scope_ctxs;
}

std::unordered_map<ScopePtr, scope_list> FunctionExtractor::PartitionIdenticalScopes(FunctionExtractor::scope_ctx_map& scope_ctxs) {
  std::unordered_map<ScopePtr, scope_list> identical_scope_map;

  for (auto& it : scope_ctxs) {
    auto scope = it.first;
    const auto& scope_ctx = it.second;
    bool unique = true;
    for (auto& kv_it : identical_scope_map) {
      auto key_scope = kv_it.first;
      const auto& key_scope_ctx = scope_ctxs[key_scope];
      auto& key_scope_vec = kv_it.second;
      if (key_scope_ctx->IsIdenticalFuncion(*scope_ctx)) {
        key_scope_vec.emplace_back(scope);
        unique = false;
        break;
      }
    }
    if (unique) {
      identical_scope_map[scope].emplace_back(scope);
    }
  }

  return identical_scope_map;
}

static bool HasSameAttribute(const Node* a, const Node* b, const c10::Symbol& attr) {
  if (!a->hasAttribute(attr) && !b->hasAttribute(attr)) {
    return true;
  }
  if (!a->hasAttribute(attr) || !b->hasAttribute(attr)) {
    return false;
  }
  auto a_kind = a->kindOf(attr);
  auto b_kind = b->kindOf(attr);
  if (a_kind != b_kind) {
    return false;
  }

#define COMP_ATTR(kind)       \
  case AttributeKind::kind: { \
    auto a_v = a->kind(attr); \
    auto b_v = b->kind(attr); \
    return a_v == b_v;        \
  }

  switch (a_kind) {
    COMP_ATTR(f)
    COMP_ATTR(fs)
    COMP_ATTR(i)
    COMP_ATTR(is)
    COMP_ATTR(s)
    COMP_ATTR(ss)
#undef COMP_ATTR
    case AttributeKind::t: {
      auto a_v = a->t(attr);
      auto b_v = b->t(attr);
      return a_v.equal(b_v);
    }
    case AttributeKind::ts: {
      auto a_v = a->ts(attr);
      auto b_v = b->ts(attr);
      return std::equal(a_v.begin(), a_v.end(), b_v.begin(), b_v.end(), [](const at::Tensor& a_t, const at::Tensor& b_t) {
        return a_t.equal(b_t);
      });
    }
    case AttributeKind::ival:
    case AttributeKind::g:
    case AttributeKind::gs:
    case AttributeKind::ty:
    case AttributeKind::tys:
    case AttributeKind::c:
    default:
      TORCH_INTERNAL_ASSERT(false, "Unexpected attribute type ", a_kind, " from node ", *a);
      break;
  }

  return true;
}

FunctionExtractor::func_ctx_map FunctionExtractor::GenFuncCtx(
    const std::unordered_map<ScopePtr, scope_list>& identical_scope_map,
    scope_ctx_map& scope_ctxs) {
  FunctionExtractor::func_ctx_map func_ctxs;

  for (auto& it : identical_scope_map) {
    const auto& scope_key = it.first;
    const auto& scope_list = it.second;

    func_ctxs.insert(std::make_pair(scope_key, new FunctionContext(scope_key, scope_list, scope_ctxs)));
  }

  return func_ctxs;
}

scope_list FunctionExtractor::SortScopesByMaxDepth(std::unordered_map<ScopePtr, scope_list>& identical_scope_map) {
  std::unordered_map<ScopePtr, size_t> scope_max_depth;
  for (const auto& it : identical_scope_map) {
    const auto& scopes = it.second;
    size_t max_depth = 0;
    for (const auto& scope : scopes) {
      if (scope->getDepth() > max_depth) {
        max_depth = scope->getDepth();
      }
    }
    scope_max_depth[it.first] = max_depth;
  }

  scope_list sorted_scopes;
  sorted_scopes.reserve(scope_max_depth.size());
  for (const auto& it : scope_max_depth) {
    sorted_scopes.emplace_back(it.first);
  }
  std::sort(
      sorted_scopes.begin(),
      sorted_scopes.end(),
      [&scope_max_depth](const ScopePtr& a, const ScopePtr& b) -> bool {
        return scope_max_depth[a] >= scope_max_depth[b];
      });
  return sorted_scopes;
}

std::pair<ValAttrNameMap, NodeAttrNameMap> FunctionExtractor::run() {
  auto scope_ctxs = PartitionNodesByScope(graph_);
  DebugPrintScopeContexts(scope_ctxs);
  auto identical_scope_map = PartitionIdenticalScopes(scope_ctxs);
  // Deepest scope comes first, guaranteeing no other scope can be its child.
  auto sorted_scope_keys = SortScopesByMaxDepth(identical_scope_map);
  for (const auto& scope_key : sorted_scope_keys) {
    if (module_names_.empty() || module_names_.find(scope_key->name().toUnqualString()) != module_names_.end()) {
      ConvertScopeToFunction(scope_key, identical_scope_map[scope_key], scope_ctxs, graph_);
    }
    GRAPH_DEBUG("Main graph afterwards: ", graph_->toString());
  }
  DebugPrintGraphWithFunction(graph_);

  // Construct return mappings
  ValAttrNameMap val_attr_to_name;
  NodeAttrNameMap node_attr_to_name;

  for (const auto& it : func_ctxs_) {
    auto func_val_map = it.second->val_attr_to_name_;
    val_attr_to_name.insert(func_val_map.begin(), func_val_map.end());
    auto func_ref_map = it.second->node_attr_to_name_;
    node_attr_to_name.insert(func_ref_map.begin(), func_ref_map.end());
  }

  // Clear
  for (auto& it : scope_ctxs) {
    delete it.second;
  }
  scope_ctxs.clear();
  for (auto& it : func_ctxs_) {
    delete it.second;
  }
  func_ctxs_.clear();

  return std::make_pair(val_attr_to_name, node_attr_to_name);
}

} // namespace

std::pair<std::unordered_map<const Value*, std::string>, std::unordered_map<const Node*, std::unordered_map<std::string, std::string>>> ONNXFunctionExtraction(std::shared_ptr<Graph>& graph, const std::vector<std::string>& module_names, const std::vector<std::string>& param_names) {
  GRAPH_UPDATE("Export these module forward calls as functions: ", module_names);
  FunctionExtractor fe(graph, module_names, param_names);
  return fe.run();
}

} // namespace onnx
} // namespace jit
} // namespace torch
