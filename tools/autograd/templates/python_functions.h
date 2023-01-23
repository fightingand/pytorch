#pragma once

#include <Python.h>

// ${generated_comment}

// Python bindings for automatically generated autograd functions

namespace torch { namespace autograd { namespace generated {

${shard_forward_declare}

inline void initialize_autogenerated_functions(PyObject* module) {
  ${shard_call}
}

}}} // namespace torch::autograd::generated
