#include <torch/csrc/cuda/Event.h>
#include <torch/csrc/cuda/Stream.h>

#include <torch/csrc/THP.h>
#include <torch/csrc/cuda/Module.h>

#include <c10/cuda/CUDAGuard.h>

#include <structmember.h>
#include <cuda_runtime_api.h>

PyObject *THCPEventClass = nullptr;

static PyObject * THCPEvent_pynew(
    PyTypeObject *type, PyObject *args, PyObject *kwargs) {
  HANDLE_TH_ERRORS
  unsigned char enable_timing = 0;
  unsigned char blocking = 0;
  unsigned char interprocess = 0;

  static char *kwlist[] =
    {"enable_timing", "blocking", "interprocess", nullptr};
  if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|bbb", kwlist,
      &enable_timing, &blocking, &interprocess)) {
    return nullptr;
  }

  THPObjectPtr ptr(type->tp_alloc(type, 0));
  if (!ptr) {
    return nullptr;
  }

  THCPEvent* self = (THCPEvent *)ptr.get();
  unsigned int flags =
    (blocking ? cudaEventBlockingSync : cudaEventDefault) |
    (enable_timing ? cudaEventDefault : cudaEventDisableTiming) |
    (interprocess ? cudaEventInterprocess : cudaEventDefault);

  new (&self->cuda_event) at::cuda::CUDAEvent(flags);

  return (PyObject *)ptr.release();
  END_HANDLE_TH_ERRORS
}

static PyObject * THCPEvent_from_ipc_handle(
    PyTypeObject *type, PyObject *args) {
  HANDLE_TH_ERRORS
  long long device_index = -1;
  const char *handle_bytes = nullptr;
  int handle_size = 0;

  // cannot use bool 'p' and bytearray 'Y' as they are not available in Python 2
  if (!PyArg_ParseTuple(
      args, "Ls#", &device_index, &handle_bytes, &handle_size)) {
    return nullptr;
  }

  AT_CHECK(handle_size == sizeof(cudaIpcEventHandle_t),
    "cudaIpcEventHandle_t expects byte-like object of size ",
    sizeof(cudaIpcEventHandle_t), ", but got ", handle_size);
  AT_CHECK(device_index >= 0, "Reconstructing event from handle requires "
    "a non-negtive device index, but got ", device_index)

  // no need to release the handle byte array as it is automatically managed
  // by the corresponding THCPEvent python object.
  // see https://docs.python.org/3/c-api/arg.html#strings-and-buffers

  THPObjectPtr ptr(type->tp_alloc(type, 0));
  if (!ptr) {
    return nullptr;
  }
  THCPEvent* self = (THCPEvent *)ptr.get();

  cudaIpcEventHandle_t handle;
  std::memcpy(&handle, handle_bytes, handle_size);
  new (&self->cuda_event) at::cuda::CUDAEvent(device_index, &handle);

  return (PyObject *)ptr.release();
  END_HANDLE_TH_ERRORS
}

static void THCPEvent_dealloc(THCPEvent *self) {
  self->cuda_event.~CUDAEvent();
  Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject * THCPEvent_get_cuda_event(THCPEvent *self) {
  HANDLE_TH_ERRORS
  return PyLong_FromVoidPtr(self->cuda_event.event());
  END_HANDLE_TH_ERRORS
}

static PyObject * THCPEvent_get_device(THCPEvent *self) {
  HANDLE_TH_ERRORS
  return THPUtils_packInt64(self->cuda_event.device_index());
  END_HANDLE_TH_ERRORS
}

static PyObject * THCPEvent_record(THCPEvent *self, THCPStream *stream) {
  HANDLE_TH_ERRORS
  self->cuda_event.record(stream->cuda_stream);
  Py_RETURN_NONE;
  END_HANDLE_TH_ERRORS
}

static PyObject * THCPEvent_wait(THCPEvent *self, THCPStream *stream) {
  HANDLE_TH_ERRORS
  with_no_gil([&] { self->cuda_event.block(stream->cuda_stream); });
  Py_RETURN_NONE;
  END_HANDLE_TH_ERRORS
}

static PyObject * THCPEvent_query(THCPEvent *self) {
  HANDLE_TH_ERRORS
  return PyBool_FromLong(self->cuda_event.query());
  END_HANDLE_TH_ERRORS
}

static PyObject * THCPEvent_elapsed_time(THCPEvent *self, THCPEvent *other) {
  HANDLE_TH_ERRORS
  return PyFloat_FromDouble(self->cuda_event.elapsed_time(other->cuda_event));
  END_HANDLE_TH_ERRORS
}

static PyObject * THCPEvent_synchronize(THCPEvent *self) {
  HANDLE_TH_ERRORS
  with_no_gil([&] { self->cuda_event.synchronize(); });
  Py_RETURN_NONE;
  END_HANDLE_TH_ERRORS
}

static PyObject * THCPEvent_ipc_handle(THCPEvent *self) {
  HANDLE_TH_ERRORS
  cudaIpcEventHandle_t handle;
  self->cuda_event.ipc_handle(&handle);
  return PyBytes_FromStringAndSize((const char *)&handle, sizeof(handle));
  END_HANDLE_TH_ERRORS
}

static struct PyGetSetDef THCPEvent_properties[] = {
  {"device", (getter)THCPEvent_get_device, nullptr, nullptr, nullptr},
  {"cuda_event", (getter)THCPEvent_get_cuda_event, nullptr, nullptr, nullptr},
  {nullptr}
};

static PyMethodDef THCPEvent_methods[] = {
  {(char*)"from_ipc_handle", (PyCFunction)THCPEvent_from_ipc_handle,
    METH_CLASS | METH_VARARGS, nullptr},
  {(char*)"record", (PyCFunction)THCPEvent_record, METH_O, nullptr},
  {(char*)"wait", (PyCFunction)THCPEvent_wait, METH_O, nullptr},
  {(char*)"query", (PyCFunction)THCPEvent_query, METH_NOARGS, nullptr},
  {(char*)"elapsed_time", (PyCFunction)THCPEvent_elapsed_time, METH_O, nullptr},
  {(char*)"synchronize", (PyCFunction)THCPEvent_synchronize,
    METH_NOARGS, nullptr},
  {(char*)"ipc_handle", (PyCFunction)THCPEvent_ipc_handle,
    METH_NOARGS, nullptr},
  {nullptr}
};

PyTypeObject THCPEventType = {
  PyVarObject_HEAD_INIT(nullptr, 0)
  "torch._C._CudaEventBase",             /* tp_name */
  sizeof(THCPEvent),                     /* tp_basicsize */
  0,                                     /* tp_itemsize */
  (destructor)THCPEvent_dealloc,         /* tp_dealloc */
  0,                                     /* tp_print */
  0,                                     /* tp_getattr */
  0,                                     /* tp_setattr */
  0,                                     /* tp_reserved */
  0,                                     /* tp_repr */
  0,                                     /* tp_as_number */
  0,                                     /* tp_as_sequence */
  0,                                     /* tp_as_mapping */
  0,                                     /* tp_hash  */
  0,                                     /* tp_call */
  0,                                     /* tp_str */
  0,                                     /* tp_getattro */
  0,                                     /* tp_setattro */
  0,                                     /* tp_as_buffer */
  Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /* tp_flags */
  nullptr,                                  /* tp_doc */
  0,                                     /* tp_traverse */
  0,                                     /* tp_clear */
  0,                                     /* tp_richcompare */
  0,                                     /* tp_weaklistoffset */
  0,                                     /* tp_iter */
  0,                                     /* tp_iternext */
  THCPEvent_methods,                     /* tp_methods */
  0,                                     /* tp_members */
  THCPEvent_properties,                  /* tp_getset */
  0,                                     /* tp_base */
  0,                                     /* tp_dict */
  0,                                     /* tp_descr_get */
  0,                                     /* tp_descr_set */
  0,                                     /* tp_dictoffset */
  0,                                     /* tp_init */
  0,                                     /* tp_alloc */
  THCPEvent_pynew,                       /* tp_new */
};

void THCPEvent_init(PyObject *module) {
  THCPEventClass = (PyObject*)&THCPEventType;
  if (PyType_Ready(&THCPEventType) < 0) {
    throw python_error();
  }
  Py_INCREF(&THCPEventType);
  if (PyModule_AddObject(
      module, "_CudaEventBase", (PyObject *)&THCPEventType) < 0) {
    throw python_error();
  }
}
