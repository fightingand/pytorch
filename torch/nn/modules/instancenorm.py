import torch
from .module import Module
from torch.nn.parameter import Parameter
from .. import functional as F


class _InstanceNorm(Module):
    def __init__(self, num_features=None, eps=1e-5):
        super(_InstanceNorm, self).__init__()
        self.num_features = num_features
        self.affine = num_features is not None
        self.eps = eps
        if self.affine:
            self.weight = Parameter(torch.ones(num_features))
            self.bias = Parameter(torch.zeros(num_features))
        else:
            self.register_parameter('weight', None)
            self.register_parameter('bias', None)
        self.reset_parameters()

    def reset_parameters(self):
        if self.affine:
            self.weight.data.fill_(1)
            self.bias.data.zero_()

    def forward(self, input):
        return F.instance_norm(input, weight=self.weight, bias=self.bias,
                            eps=self.eps)

    def __repr__(self):
        if self.affine:
            return ('{name}({num_features}, eps={eps})'
                    .format(name=self.__class__.__name__, **self.__dict__))
        else:
            return ('{name}(eps={eps})'
                    .format(name=self.__class__.__name__, **self.__dict__))


class LayerNorm(_InstanceNorm):
    r"""Applies Layer Normalization over a 2D input that is seen
    as a mini-batch of 1D inputs.

    .. math::

        y = \gamma * \frac{x - \mu_x}{\sigma_x + \epsilon} + \beta

    The mean and standard deviation are calculated per-dimension separately
    for each object in a mini-batch (over `num_features`). Gamma and beta are
    optional learnable parameter vectors of size C (where C is the input size).

    Args:
        num_features: num_features from an expected input of size
            `batch_size x num_features`. Specified only if learnable parameters
            are desired. Default: None
        eps: a value added to the denominator for numerical stability.
            Default: 1e-5

    Shape:
        - Input: :math:`(N, C)`
        - Output: :math:`(N, C)` (same shape as input)

    Examples:
        >>> # Without Learnable Parameters
        >>> m = nn.LayerNorm()
        >>> # With Learnable Parameters
        >>> m = nn.LayerNorm(100)
        >>> input = autograd.Variable(torch.randn(20, 100))
        >>> output = m(input)
    """

    # Assuming 2D inputs, no need to manipulate dimensions
    def forward(self, input):
        mean = input.mean(1, keepdim=True)
        std = input.std(1, keepdim=True)
        output = (x - mean) / (std + self.eps)
        if self.affine:
            if input.size(1) != self.weight.nelement():
                raise RuntimeError('got {}-feature tensor, expected {}'
                                   .format(input.size(1),
                                           self.weight.nelement()))
            output = self.weight * output + self.bias
        return output

    def _check_input_dim(self, input):
        if input.dim() != 2:
            raise ValueError('expected 2D input (got {}D input)'
                             .format(input.dim()))
        super(LayerNorm, self)._check_input_dim(input)


class InstanceNorm1d(_LayerNorm):
    r"""Applies Instance Normalization over a 3D input that is seen
    as a mini-batch of 2D inputs.

    .. math::

        y = \gamma * \frac{x - \mu_x}{\sigma_x + \epsilon} + \beta

    The mean and standard deviation are calculated per-dimension separately
    for each object in a mini-batch. Gamma and beta are optional learnable
    parameter vectors of size C (where C is the input size). This can be seen as
    an extension of layer normalization where statistics are not calculated over
    `batch_size` or `num_features`.

    Args:
        num_features: num_features from an expected input of size
            `batch_size x num_features x length`. Specified only if learnable
            parameters are desired. Default: None
        eps: a value added to the denominator for numerical stability.
            Default: 1e-5

    Shape:
        - Input: :math:`(N, C, L)`
        - Output: :math:`(N, C, L)` (same shape as input)

    Examples:
        >>> # Without Learnable Parameters
        >>> m = nn.InstanceNorm1d()
        >>> # With Learnable Parameters
        >>> m = nn.InstanceNorm1d(100)
        >>> input = autograd.Variable(torch.randn(20, 100, 40))
        >>> output = m(input)
    """

    def _check_input_dim(self, input):
        if input.dim() != 3:
            raise ValueError('expected 3D input (got {}D input)'
                             .format(input.dim()))
        super(InstanceNorm1d, self)._check_input_dim(input)


class InstanceNorm2d(_LayerNorm):
    r"""Applies Instance Normalization over a 4D input that is seen as a
    mini-batch of 3D inputs.

    .. math::

        y = \gamma * \frac{x - \mu_x}{\sigma_x + \epsilon} + \beta

    The mean and standard deviation are calculated per-dimension separately
    for each object in a mini-batch. Gamma and beta are optional learnable
    parameter vectors of size C (where C is the input size). This can be seen as
    an extension of layer normalization where statistics are not calculated over
    `batch_size` or `num_features`.

    Args:
        num_features: num_features from an expected input of size
            `batch_size x num_features x height x width`. Specified only if
            learnable parameters are desired. Default: None
        eps: a value added to the denominator for numerical stability.
            Default: 1e-5
    Shape:
        - Input: :math:`(N, C, H, W)`
        - Output: :math:`(N, C, H, W)` (same shape as input)

    Examples:
        >>> # Without Learnable Parameters
        >>> m = nn.InstanceNorm2d()
        >>> # With Learnable Parameters
        >>> m = nn.InstanceNorm2d(100)
        >>> input = autograd.Variable(torch.randn(20, 100, 35, 45))
        >>> output = m(input)
    """

    def _check_input_dim(self, input):
        if input.dim() != 4:
            raise ValueError('expected 4D input (got {}D input)'
                             .format(input.dim()))
        super(InstanceNorm2d, self)._check_input_dim(input)


class InstanceNorm3d(_LayerNorm):
    r"""Applies Instance Normalization over a 5D input that is seen as a
    mini-batch of 4D inputs

    .. math::

        y = \gamma * \frac{x - \mu_x}{\sigma_x + \epsilon} + \beta

    The mean and standard deviation are calculated per-dimension separately
    for each object in a mini-batch. Gamma and beta are optional learnable
    parameter vectors of size C (where C is the input size). This can be seen as
    an extension of layer normalization where statistics are not calculated over
    `batch_size` or `num_features`.

    Args:
        num_features: num_features from an expected input of size
            `batch_size x num_features x depth x height x width`. Specified only
            if learnable parameters are desired. Default: None
        eps: a value added to the denominator for numerical stability.
            Default: 1e-5

    Shape:
        - Input: :math:`(N, C, D, H, W)`
        - Output: :math:`(N, C, D, H, W)` (same shape as input)

    Examples:
        >>> # Without Learnable Parameters
        >>> m = nn.InstanceNorm3d()
        >>> # With Learnable Parameters
        >>> m = nn.InstanceNorm3d(100)
        >>> input = autograd.Variable(torch.randn(20, 100, 35, 45, 10))
        >>> output = m(input)
    """

    def _check_input_dim(self, input):
        if input.dim() != 5:
            raise ValueError('expected 5D input (got {}D input)'
                             .format(input.dim()))
        super(InstanceNorm3d, self)._check_input_dim(input)
