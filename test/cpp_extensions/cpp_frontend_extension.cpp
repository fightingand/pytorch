#include <torch/extension.h>

#include <cstddef>
#include <string>

struct Net : torch::nn::Module {
  using torch::nn::Module::register_parameter;

  Net(int64_t in, int64_t out) : fc(in, out) {
    register_module("fc", fc);
    buffer = register_buffer("buf", torch::eye(5));
  }

  torch::Tensor forward(torch::Tensor x) {
    return fc->forward(x);
  }

  void set_bias(torch::Tensor bias) {
    torch::NoGradGuard guard;
    fc->bias.set_(bias);
  }

  torch::Tensor get_bias() const {
    return fc->bias;
  }

  void add_new_parameter(const std::string& name, torch::Tensor tensor) {
    register_parameter(name, tensor);
  }

  void add_new_buffer(const std::string& name, torch::Tensor tensor) {
    register_buffer(name, tensor);
  }

  void add_new_submodule(const std::string& name) {
    register_module(name, torch::nn::Linear(fc->options));
  }

  torch::nn::Linear fc;
  torch::Tensor buffer;
};

PYBIND11_MODULE(TORCH_EXTENSION_NAME, m) {
  torch::python::bind_module<Net>(m, "Net")
      .def(py::init<int64_t, int64_t>())
      .def("set_bias", &Net::set_bias)
      .def("get_bias", &Net::get_bias)
      .def("add_new_parameter", &Net::add_new_parameter)
      .def("add_new_buffer", &Net::add_new_buffer)
      .def("add_new_submodule", &Net::add_new_submodule);
}
