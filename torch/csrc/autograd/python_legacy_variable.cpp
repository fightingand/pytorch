#include <torch/csrc/autograd/python_legacy_variable.h>

#include <ATen/ATen.h>

#include <torch/csrc/Exceptions.h>
#include <torch/csrc/autograd/python_function.h>
#include <torch/csrc/autograd/python_variable.h>
#include <torch/csrc/tensor/python_tensor.h>
#include <torch/csrc/jit/tracer.h>

using namespace at;

namespace torch { namespace autograd {

static PyObject *THPVariable_pynew(PyTypeObject* type, PyObject *args, PyObject *kwds) {
  HANDLE_TH_ERRORS
  THPObjectPtr _data;
  PyObject *data = nullptr;
  PyObject *grad_fn = nullptr;
  char is_volatile = 0;
  char requires_grad = 0;
  const char* name = nullptr;

  const char *accepted_args[] = {"data", "requires_grad", "volatile", "_grad_fn", "name", nullptr};
  if (!PyArg_ParseTupleAndKeywords(args, kwds, "|ObbOz", (char**)accepted_args,
      &data, &requires_grad, &is_volatile, &grad_fn, &name))
    return nullptr;

  if (grad_fn == Py_None)
    grad_fn = nullptr;

  if (is_volatile) {
    PyErr_WarnEx(PyExc_UserWarning,
        "volatile was removed and now has no effect. Use `with torch.no_grad():` "
        "instead.", 1);
  }

  if (is_volatile && requires_grad) {
    throw ValueError("Variable can't be volatile and require_grad at the same time!");
  }
  if (grad_fn && !THPFunction_Check(grad_fn)) {
    throw TypeError("_grad_fn has to be a Function object or None, but got %s",
        Py_TYPE(grad_fn)->tp_name);
  }
  Variable var;
  if (!data || data == Py_None) {
    // For legacy serialization code, create an empty tensor. This is also used
    // by nn.Parameter() with no arguments.
    auto type_id = torch::tensors::get_default_tensor_type_id();
    auto scalar_type = torch::tensors::get_default_scalar_type();
    auto options = TensorOptions(scalar_type)
        .device(computeDeviceType(type_id))
        .layout(layout_from_backend(tensorTypeIdToBackend(type_id)));
    var = at::empty({0}, options);
  } else if (THPVariable_Check(data)) {
    var = ((THPVariable*)data)->cdata.detach();
  } else {
    throw torch::TypeError("Variable data has to be a tensor, but got %s",
        Py_TYPE(data)->tp_name);
  }
  // We set `tensor`'s `allow_tensor_metadata_change` to true here, because we want to
  // allow the following use case for backward compatibility:
  //
  // ```python
  // var = Variable(torch.randn(2, 3))
  // var.resize_(4, 5)
  // ```
  var.unsafeGetTensorImpl()->set_allow_tensor_metadata_change(true);

  TORCH_CHECK(!grad_fn,
    "_grad_fn argument to legacy Variable constructor is no longer supported.  "
    "Instead, please invoke your _grad_fn to produce a variable with it as the "
    "_grad_fn.");
  var.set_requires_grad(requires_grad);

  if (name) {
    impl::set_name(var, name);
  }

  if (jit::tracer::isTracing() && data && data != Py_None && THPVariable_Check(data)) {
    if (auto *v = jit::tracer::getValueTrace(((THPVariable*)data)->cdata)) {
      jit::tracer::setValueTrace(var, v);
    }
  }

  return THPVariable_Wrap(std::move(var));
  END_HANDLE_TH_ERRORS
}

PyTypeObject THPLegacyVariableType = {
  PyVarObject_HEAD_INIT(nullptr, 0)
  "torch._C._LegacyVariableBase",              /* tp_name */
  0,                                           /* tp_basicsize */
  0,                                           /* tp_itemsize */
  nullptr,                                     /* tp_dealloc */
  0,                                           /* tp_vectorcall_offset */
  nullptr,                                     /* tp_getattr */
  nullptr,                                     /* tp_setattr */
  nullptr,                                     /* tp_reserved */
  nullptr,                                     /* tp_repr */
  nullptr,                                     /* tp_as_number */
  nullptr,                                     /* tp_as_sequence */
  nullptr,                                     /* tp_as_mapping */
  nullptr,                                     /* tp_hash  */
  nullptr,                                     /* tp_call */
  nullptr,                                     /* tp_str */
  nullptr,                                     /* tp_getattro */
  nullptr,                                     /* tp_setattro */
  nullptr,                                     /* tp_as_buffer */
  Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,    /* tp_flags */
  nullptr,                                     /* tp_doc */
  nullptr,                                     /* tp_traverse */
  nullptr,                                     /* tp_clear */
  nullptr,                                     /* tp_richcompare */
  0,                                           /* tp_weaklistoffset */
  nullptr,                                     /* tp_iter */
  nullptr,                                     /* tp_iternext */
  nullptr,                                     /* tp_methods */
  nullptr,                                     /* tp_members */
  nullptr,                                     /* tp_getset */
  nullptr,                                     /* tp_base */
  nullptr,                                     /* tp_dict */
  nullptr,                                     /* tp_descr_get */
  nullptr,                                     /* tp_descr_set */
  0,                                           /* tp_dictoffset */
  nullptr,                                     /* tp_init */
  nullptr,                                     /* tp_alloc */
  THPVariable_pynew                            /* tp_new */
};

void init_legacy_variable(PyObject *module) {
  if (PyType_Ready(&THPLegacyVariableType) < 0) {
    throw python_error();
  }
  auto obj = (PyObject*)&THPLegacyVariableType;
  Py_INCREF(obj);
  if (PyModule_AddObject(module, "_LegacyVariableBase", obj) < 0) {
    throw python_error();
  }
}

}}  // namespace torch::autograd
