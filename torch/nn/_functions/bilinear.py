import torch
from torch.autograd import Function


class Bilinear(Function):

    def forward(self, input1, input2, weight, bias=None):
        self.save_for_backward(input1, input2, weight, bias)

        output = input1.new(input1.size(0), weight.size(0))

        buff = input1.new()

        # compute output scores:
        for k, w in enumerate(weight):
            torch.mm(input1, w, out=buff)
            buff.mul_(input2)
            torch.sum(buff, 1, out=output.narrow(1, k, 1))

        if bias is not None:
            output.add_(bias.expand_as(output))

        return output

    def backward(self, grad_output):
        input1, input2, weight, bias = self.saved_tensors
        grad_input1 = grad_input2 = grad_weight = grad_bias = None

        buff = input1.new()

        if self.needs_input_grad[0] or self.needs_input_grad[1]:
            grad_input1 = torch.mm(input2, weight[0].t())
            grad_input1.mul_(grad_output.narrow(1, 0, 1).expand(grad_input1.size()))
            grad_input2 = torch.mm(input1, weight[0])
            grad_input2.mul_(grad_output.narrow(1, 0, 1).expand(grad_input2.size()))

            for k in range(1, weight.size(0)):
                torch.mm(input2, weight[k].t(), out=buff)
                buff.mul_(grad_output.narrow(1, k, 1).expand(grad_input1.size()))
                grad_input1.add_(buff)

                torch.mm(input1, weight[k], out=buff)
                buff.mul_(grad_output.narrow(1, k, 1).expand(grad_input2.size()))
                grad_input2.add_(buff)

        if self.needs_input_grad[2]:
            # accumulate parameter gradients:
            for k in range(weight.size(0)):
                torch.mul(input1, grad_output.narrow(1, k, 1).expand_as(input1), out=buff)
            grad_weight = torch.mm(buff.t(), input2)

        if bias is not None and self.needs_input_grad[3]:
            grad_bias = grad_output.sum(0)

        return grad_input1, grad_input2, grad_weight, grad_bias
