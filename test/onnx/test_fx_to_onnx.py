# Owner(s): ["module: onnx"]
import onnx
import pytorch_test_common
import torch
from torch import nn
from torch.nn import functional as F
from torch.onnx._internal import fx as fx_onnx
from torch.testing._internal import common_utils


class TestFxToOnnx(pytorch_test_common.ExportTestCase):
    def setUp(self):
        super().setUp()
        self.opset_version = torch.onnx._constants.ONNX_DEFAULT_OPSET

    def test_simple_function(self):
        def func(x):
            y = x + 1
            z = y.relu()
            return (y, z)

        onnx_model = fx_onnx.export(func, torch.randn(1, 1, 2), opset_version=self.opset_version)
        onnx.checker.check_model(onnx_model)

    def test_mnist(self):
        class MNISTModel(nn.Module):
            def __init__(self):
                super().__init__()
                self.conv1 = nn.Conv2d(1, 32, 3, 1, bias=False)
                self.conv2 = nn.Conv2d(32, 64, 3, 1, bias=False)
                self.fc1 = nn.Linear(9216, 128, bias=False)
                self.fc2 = nn.Linear(128, 10, bias=False)

            def forward(self, tensor_x: torch.Tensor):
                tensor_x = self.conv1(tensor_x)
                tensor_x = F.sigmoid(tensor_x)
                tensor_x = self.conv2(tensor_x)
                tensor_x = F.sigmoid(tensor_x)
                tensor_x = F.max_pool2d(tensor_x, 2)
                tensor_x = torch.flatten(tensor_x, 1)
                tensor_x = self.fc1(tensor_x)
                tensor_x = F.sigmoid(tensor_x)
                tensor_x = self.fc2(tensor_x)
                output = F.log_softmax(tensor_x, dim=1)
                return output

        tensor_x = torch.rand((64, 1, 28, 28), dtype=torch.float32)
        onnx_model = fx_onnx.export(MNISTModel(), tensor_x, opset_version=self.opset_version)
        onnx.checker.check_model(onnx_model, full_check=True)


if __name__ == "__main__":
    common_utils.run_tests()
