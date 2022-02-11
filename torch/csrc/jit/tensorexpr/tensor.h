#pragma once

#include <torch/csrc/Export.h>
#include <functional>
#include <vector>

#include <torch/csrc/jit/tensorexpr/dim_arg.h>
#include <torch/csrc/jit/tensorexpr/expr.h>
#include <torch/csrc/jit/tensorexpr/reduction.h>

namespace torch {
namespace jit {
namespace tensorexpr {

class TORCH_API Tensor {
 public:
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
  Tensor(BufPtr buf, const std::vector<VarPtr>& args, ExprPtr body)
      : buf_(buf) {
    stmt_ = constructStmt(args, body, {}, {});
  }
  Tensor(BufHandle buf, const std::vector<VarHandle>& args, ExprHandle body)
      : Tensor(buf.node(), VarHandleVectorToVarVector(args), body.node()) {}

  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
  Tensor(
      BufPtr buf,
      const std::vector<VarPtr>& args,
      const std::vector<ExprPtr>& reduce_dims,
      const std::vector<VarPtr>& reduce_args,
      ExprPtr body)
      : buf_(buf) {
    stmt_ = constructStmt(args, body, reduce_dims, reduce_args);
  }
  Tensor(
      BufHandle buf,
      const std::vector<VarHandle>& args,
      const std::vector<ExprHandle>& reduce_dims,
      const std::vector<VarHandle>& reduce_args,
      ExprHandle body)
      : Tensor(
            buf.node(),
            VarHandleVectorToVarVector(args),
            ExprHandleVectorToExprVector(reduce_dims),
            VarHandleVectorToVarVector(reduce_args),
            body.node()) {}

  Tensor(BufPtr buf, StmtPtr stmt) : buf_(buf), stmt_(stmt) {}

  BufPtr buf() const {
    return buf_;
  }

  StmtPtr stmt() const {
    return stmt_;
  }

  template <typename T>
  inline ExprHandle load(const std::vector<T>& args) const;
  template <typename... Ts>
  inline ExprHandle load(const Ts&... ts) const;

 private:
  StmtPtr constructStmt(
      const std::vector<VarPtr>& args,
      ExprPtr body,
      const std::vector<ExprPtr>& reduce_dims,
      const std::vector<VarPtr>& reduce_args) const;

  BufPtr buf_;
  StmtPtr stmt_;
};

TORCH_API Tensor Compute(
    const std::string& func_name,
    const std::vector<DimArg>& dim_args,
    const std::function<ExprHandle(const VarHandle&)>& body_func);
TORCH_API Tensor Compute(
    const std::string& func_name,
    const std::vector<DimArg>& dim_args,
    const std::function<ExprHandle(const VarHandle&, const VarHandle&)>&
        body_func);
TORCH_API Tensor Compute(
    const std::string& func_name,
    const std::vector<DimArg>& dim_args,
    const std::function<
        ExprHandle(const VarHandle&, const VarHandle&, const VarHandle&)>&
        body_func);
TORCH_API Tensor Compute(
    const std::string& func_name,
    const std::vector<DimArg>& dim_args,
    const std::function<ExprHandle(
        const VarHandle&,
        const VarHandle&,
        const VarHandle&,
        const VarHandle&)>& body_func);
TORCH_API Tensor Compute(
    const std::string& func_name,
    const std::vector<DimArg>& dim_args,
    const std::function<ExprHandle(const std::vector<VarHandle>&)>& body_func);

inline void unpack_dim_args(
    const std::vector<DimArg>& dim_args,
    std::vector<ExprHandle>* dims,
    std::vector<VarHandle>* vars) {
  dims->clear();
  vars->clear();
  for (const DimArg& dim_arg : dim_args) {
    ExprHandle expr = dim_arg.dim();
    dims->push_back(expr);
    vars->push_back(VarHandle(alloc<Var>(
        dim_arg.name_hint(),
        expr.dtype().scalar_type() == ScalarType::Long ? kLong : kInt)));
  }
}

// Handle reductions over a Reducer and a body_func which produces values.
template <typename InitFunc, typename BodyFunc>
Tensor Reduce(
    const std::string& func_name,
    const std::vector<DimArg>& dim_args,
    const Reducer& reducer,
    const InitFunc& init_func,
    const BodyFunc& body_func,
    const std::vector<DimArg>& reduce_args) {
  std::vector<ExprHandle> dims;
  std::vector<VarHandle> vars;
  unpack_dim_args(dim_args, &dims, &vars);

  std::vector<ExprHandle> reduce_dims;
  std::vector<VarHandle> reduce_vars;
  unpack_dim_args(reduce_args, &reduce_dims, &reduce_vars);

  // If reduce_vars is empty, then it's not a reduction, but rather a simple
  // copy
  if (reduce_vars.empty()) {
    ExprHandle body = Reducer::getReduceBody(body_func, vars);
    BufHandle func_result = Buf::make(func_name, dims, body.dtype());
    return Tensor(func_result, vars, body);
  }

  std::vector<VarHandle> all_vars;
  all_vars.insert(all_vars.end(), vars.begin(), vars.end());
  all_vars.insert(all_vars.end(), reduce_vars.begin(), reduce_vars.end());

  ExprHandle body = Reducer::getReduceBody(body_func, all_vars);
  std::vector<ExprHandle> output_args(vars.begin(), vars.end());
  ExprHandle init_expr = Cast::make(body.dtype(), init_func(vars));
  BufHandle func_result = Buf::make(func_name, dims, body.dtype(), init_expr);
  ExprHandle reduce_op = reducer(func_result, body, output_args, reduce_vars);
  Tensor t = Tensor(func_result, vars, reduce_dims, reduce_vars, reduce_op);
  return t;
}

template <typename BodyFunc>
Tensor Reduce(
    const std::string& func_name,
    const std::vector<DimArg>& dim_args,
    const Reducer& reducer,
    const BodyFunc& body_func,
    const std::vector<DimArg>& reduce_args) {
  return Reduce(
      func_name,
      dim_args,
      reducer,
      [&](ParameterList p) { return ExprHandle(reducer.initializer()); },
      body_func,
      reduce_args);
}

// Overload which allows inline lambda functions for the body_func.
template <typename BodyFunc>
Tensor Reduce(
    const std::string& func_name,
    const std::vector<DimArg>& dim_args,
    const Reducer& reducer,
    const BodyFunc&& body_func,
    const std::vector<DimArg>& reduce_args) {
  return Reduce(func_name, dim_args, reducer, body_func, reduce_args);
}

TORCH_API Tensor Reduce(
    const std::string& name,
    const std::vector<DimArg>& dim_args,
    const Reducer& reducer,
    const BufHandle& buffer,
    const std::vector<DimArg>& reduce_args);

// Overload for the common case of all dimensions of a prevously Computed
// Tensor.
TORCH_API Tensor Reduce(
    const std::string& func_name,
    const std::vector<DimArg>& dim_args,
    const Reducer& reducer,
    Tensor tensor,
    const std::vector<DimArg>& reduce_args);

template <typename... Ts>
inline ExprHandle Tensor::load(const Ts&... ts) const {
  // NOLINTNEXTLINE(cppcoreguidelines-init-variables)
  std::vector<ExprHandle> params({ExprHandle(ts)...});
  return Load::make(BufHandle(this->buf()), params);
}

template <typename T>
inline ExprHandle Tensor::load(const std::vector<T>& args) const {
  // NOLINTNEXTLINE(cppcoreguidelines-init-variables)
  std::vector<ExprHandle> params(args.begin(), args.end());
  return Load::make(BufHandle(this->buf()), params);
}

template <typename... Ts>
inline ExprHandle BufHandle::load(const Ts&... ts) const {
  // NOLINTNEXTLINE(cppcoreguidelines-init-variables)
  std::vector<ExprHandle> params({ExprHandle(ts)...});
  return ExprHandle(alloc<Load>(node(), ExprHandleVectorToExprVector(params)));
}

template <typename T>
inline ExprHandle BufHandle::load(const std::vector<T>& args) const {
  // NOLINTNEXTLINE(cppcoreguidelines-init-variables)
  std::vector<ExprHandle> params(args.begin(), args.end());
  return ExprHandle(alloc<Load>(node(), ExprHandleVectorToExprVector(params)));
}

inline ExprHandle BufHandle::load(const std::vector<ExprHandle>& args) const {
  return this->template load<ExprHandle>(args);
}

} // namespace tensorexpr
} // namespace jit
} // namespace torch
