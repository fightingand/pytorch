#include "torch/csrc/autograd/generated/python_functions.h"

// ${generated_comment}

#include <Python.h>
#include <ATen/ATen.h>

#include "torch/csrc/autograd/generated/Functions.h"
#include "torch/csrc/autograd/python_cpp_function.h"

namespace torch { namespace autograd { namespace generated {

template<typename C>
static void addClass(PyTypeObject& type, const char* name,
  PyGetSetDef* function_properties=NULL, PyMethodDef* function_methods=NULL)
{
  _initFunctionPyTypeObject(type, name, function_properties, function_methods);
  Py_INCREF(&type);
  registerCppFunction(typeid(C), &type);
}

void initialize_autogenerated_functions() {
  ${py_function_initializers}
}

}}} // namespace torch::autograd::generated
