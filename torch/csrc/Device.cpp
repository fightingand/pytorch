#include "torch/csrc/Device.h"

#include "torch/csrc/Exceptions.h"
#include "torch/csrc/utils/object_ptr.h"
#include "torch/csrc/utils/python_arg_parser.h"
#include "torch/csrc/utils/python_strings.h"
#include "torch/csrc/utils/python_numbers.h"
#include "torch/csrc/utils/pybind.h"

#include <ATen/Device.h>
#include <ATen/Error.h>

#include <cstring>
#include <structmember.h>
#include <sstream>

PyObject *THPDevice_New(const at::Device& device)
{
  auto type = (PyTypeObject*)&THPDeviceType;
  auto self = THPObjectPtr{type->tp_alloc(type, 0)};
  if (!self) throw python_error();
  auto self_ = reinterpret_cast<THPDevice*>(self.get());
  self_->device = device;
  return self.release();
}

PyObject *THPDevice_repr(THPDevice *self)
{
  std::ostringstream oss;
  oss << "device(type=\'" << self->device.type() << "\'";
  if (self->device.has_index()) {
    oss << ", index=" << self->device.index();
  }
  oss << ")";
  return THPUtils_packString(oss.str().c_str());
}

PyObject *THPDevice_str(THPDevice *self)
{
  std::ostringstream oss;
  oss << self->device;
  return THPUtils_packString(oss.str().c_str());
}

PyObject *THPDevice_pynew(PyTypeObject *type, PyObject *args, PyObject *kwargs)
{
  HANDLE_TH_ERRORS
  static torch::PythonArgParser parser({
    "Device(Device device)",
    "Device(std::string type, int64_t? index=-1)"
  });
  torch::ParsedArgs<2> parsed_args;
  auto r = parser.parse(args, kwargs, parsed_args);
  if (r.idx == 0) {
    auto device = r.device(0);
    return THPDevice_New(device);
  } else if (r.idx == 1) {
    auto as_device = r.device(0);  // this works, because device can take strings
    auto device_type = r.string(0);
    if (as_device.has_index()) {
      throw std::runtime_error("type (string) must not include an index because index "
                                "was passed explicitly: " + device_type);
    }
    int32_t device_index = -1;
    if (!r.isNone(1)) {
      device_index = r.toInt64(1);
      // -1 is allowed in ATen/C++, to mean the default device, but not in
      // Python.
      AT_CHECK(device_index >= 0, "Device index must not be negative");
    }
    at::Device device(as_device.type(), device_index);
    return THPDevice_New(device);
  }
  Py_RETURN_NONE;
  END_HANDLE_TH_ERRORS
}

PyObject *THPDevice_type(THPDevice *self)
{
  HANDLE_TH_ERRORS
  std::ostringstream oss;
  oss << self->device.type();
  return THPUtils_packString(oss.str().c_str());
  Py_RETURN_NONE;
  END_HANDLE_TH_ERRORS
}

PyObject *THPDevice_index(THPDevice *self)
{
  HANDLE_TH_ERRORS
  if (self->device.has_index()) {
    return THPUtils_packInt64(self->device.index());
  } else {
    Py_RETURN_NONE;
  }
  END_HANDLE_TH_ERRORS
}

static size_t THPDevice_hash(THPDevice *self)
{
  HANDLE_TH_ERRORS
  return std::hash<at::Device>{}(self->device);
  END_HANDLE_TH_ERRORS_RET(-1)
}

PyObject *THPDevice_rc(PyObject *a, PyObject *b, int op) {
  HANDLE_TH_ERRORS
  if (!THPDevice_Check(a) || !THPDevice_Check(b)) {
    // Py_RETURN_NOTIMPLEMENTED not in python 2.
    Py_INCREF(Py_NotImplemented);
    return Py_NotImplemented;
  }
  THPDevice *da = reinterpret_cast<THPDevice*>(a);
  THPDevice *db = reinterpret_cast<THPDevice*>(b);

  switch(op) {
    case Py_EQ:
      if (da->device == db->device) {
        Py_RETURN_TRUE;
      } else {
        Py_RETURN_FALSE;
      }
    case Py_NE:
      if (da->device == db->device) {
        Py_RETURN_FALSE;
      } else {
        Py_RETURN_TRUE;
      }
    case Py_LT:
    case Py_LE:
    case Py_GT:
    case Py_GE:
      throw torch::TypeError("comparison not implemented");
    default:
      throw torch::TypeError("unexpected comparison op");
  }
  END_HANDLE_TH_ERRORS
}

PyObject *THPDevice_reduce(THPDevice *self)
{
  HANDLE_TH_ERRORS
  auto ret = THPObjectPtr{PyTuple_New(2)};
  if (!ret) throw python_error();

  py::object torch_module = py::module::import("torch");
  py::object torch_device = torch_module.attr("device");
  PyTuple_SET_ITEM(ret.get(), 0, torch_device.release().ptr());

  THPObjectPtr args;
  std::ostringstream oss;
  oss << self->device.type();
  if (self->device.has_index()) {
    args = THPObjectPtr{Py_BuildValue("(si)", oss.str().c_str(), self->device.index())};
  } else {
    args = THPObjectPtr{Py_BuildValue("(s)", oss.str().c_str())};
  }
  if (!args) throw python_error();
  PyTuple_SET_ITEM(ret.get(), 1, args.release());

  return ret.release();
  END_HANDLE_TH_ERRORS
}

typedef PyObject *(*getter)(PyObject *, void *);

static struct PyGetSetDef THPDevice_properties[] = {
  {"type",       (getter)THPDevice_type, nullptr, nullptr, nullptr},
  {"index",      (getter)THPDevice_index, nullptr, nullptr, nullptr},
  {nullptr}
};

static PyMethodDef THPDevice_methods[] = {
  {"__reduce__", (PyCFunction)THPDevice_reduce, METH_NOARGS, nullptr},
  {NULL}  /* Sentinel */
};

PyTypeObject THPDeviceType = {
  PyVarObject_HEAD_INIT(nullptr, 0)
  "torch.device",                        /* tp_name */
  sizeof(THPDevice),                     /* tp_basicsize */
  0,                                     /* tp_itemsize */
  0,                                     /* tp_dealloc */
  0,                                     /* tp_print */
  0,                                     /* tp_getattr */
  0,                                     /* tp_setattr */
  0,                                     /* tp_reserved */
  (reprfunc)THPDevice_repr,              /* tp_repr */
  0,                                     /* tp_as_number */
  0,                                     /* tp_as_sequence */
  0,                                     /* tp_as_mapping */
  (hashfunc)THPDevice_hash,              /* tp_hash  */
  0,                                     /* tp_call */
  (reprfunc)THPDevice_str,               /* tp_str */
  0,                                     /* tp_getattro */
  0,                                     /* tp_setattro */
  0,                                     /* tp_as_buffer */
  Py_TPFLAGS_DEFAULT,                    /* tp_flags */
  nullptr,                               /* tp_doc */
  0,                                     /* tp_traverse */
  0,                                     /* tp_clear */
  (richcmpfunc)THPDevice_rc,             /* tp_richcompare */
  0,                                     /* tp_weaklistoffset */
  0,                                     /* tp_iter */
  0,                                     /* tp_iternext */
  THPDevice_methods,                     /* tp_methods */
  0,                                     /* tp_members */
  THPDevice_properties,                  /* tp_getset */
  0,                                     /* tp_base */
  0,                                     /* tp_dict */
  0,                                     /* tp_descr_get */
  0,                                     /* tp_descr_set */
  0,                                     /* tp_dictoffset */
  0,                                     /* tp_init */
  0,                                     /* tp_alloc */
  THPDevice_pynew,                       /* tp_new */
};

void THPDevice_init(PyObject *module)
{
  if (PyType_Ready(&THPDeviceType) < 0) {
    throw python_error();
  }
  Py_INCREF(&THPDeviceType);
  if (PyModule_AddObject(module, "device", (PyObject *)&THPDeviceType) != 0) {
    throw python_error();
  }
}
