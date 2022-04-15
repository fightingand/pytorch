#pragma once

// ${generated_comment}

#include <c10/core/QScheme.h>
#include <ATen/core/jit_type_base.h>
#include <tuple>
#include <vector>


// Forward declarations of any types needed in the operator signatures.
// We can't directly include these classes because it will cause circular include dependencies.
// This file is included by TensorBody.h, which defines the Tensor class.
namespace c10 {

template<typename T>
class optional;
template<typename T>
class List;
template<typename T>
class IListRef;
class Stream;
class Scalar;
struct Storage;
struct TensorOptions;

}

namespace at {

class Tensor;
class OptionalTensorRef;
struct Dimname;
struct Generator;
using TensorList = c10::ArrayRef<Tensor>;
using ITensorListRef = c10::IListRef<Tensor>;
using IOptTensorListRef = c10::IListRef<OptionalTensorRef>;
using DimnameList = c10::ArrayRef<Dimname>;
using c10::Stream;
using c10::Storage;
using c10::QScheme;
using c10::Scalar;
using c10::TensorOptions;

namespace _ops {

${declarations}

}} // namespace at::_ops
