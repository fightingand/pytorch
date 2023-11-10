import functools
import torch
import torch.distributed as dist
from typing import Optional


class DefaultState:
    """
    Stores state needed to perform the default communication algorithm within a communication hook.

    Args:
        process_group (ProcessGroup): The process group to be used.
    """

    __slots__ = [
        "process_group",
        "world_size",
        "gradient_predivide_factor",
        "gradient_postdivide_factor"
    ]

    def __init__(
        self,
        process_group: dist.ProcessGroup
    ):
        if process_group is None:
            raise ValueError(f"Expected to pass in an explicit ProcessGroup to {self}.")
        self.process_group = process_group
        self.world_size = dist.get_world_size(process_group)
        # Setting two factors `self.gradient_predivide_factor`
        # and `self.gradient_postdivide_factor` to avoid underflow and overflow
        self.gradient_predivide_factor = self._get_gradient_predivide_factor(
            self.world_size
        )
        self.gradient_postdivide_factor = self.world_size / self.gradient_predivide_factor

    @staticmethod
    def _get_gradient_predivide_factor(world_size: int) -> float:
        factor: int = 1
        while world_size % factor == 0 and world_size / factor > factor:
            factor *= 2
        return float(factor)

class LowPrecisionState(DefaultState):
    """Stores state needed to perform gradient communication in a lower precision within a communication hook.

    Communication hook will cast gradients back to the original parameter precision
    specified by ``parameter_type`` (default: torch.float32).
    Builds on top of the :class:`DefaultState`.

    Args:
        parameter_type (torch.dtype): The precision of model's parameters.
        Required for a hook to cast gradients back to a parameter's precision.
    """

    __slots__ = [
        "parameter_type",
    ]

    def __init__(
        self,
        process_group,
        parameter_type=torch.float32,
    ):
        super().__init__(process_group)
        self.parameter_type = parameter_type


def _decompress(state: LowPrecisionState, grad: torch.Tensor):
    """
    Casts gradients back to full parameter precision so that further computation happens in full precision.

    Args:
        state (LowPrecisionState): The state containing information about parameter precision.
        grad (torch.Tensor): The gradients to be cast back to full precision.
    """
    orig_grad_data = grad.data
    grad.data = grad.data.to(state.parameter_type)
    # Don't let this memory get reused until after the transfer.
    orig_grad_data.record_stream(torch.cuda.current_stream())  # type: ignore[arg-type]

def allreduce_hook(state: DefaultState, grad: torch.Tensor):
    """
    Implement the 'all_reduce' algorithm and perform pre- and post-divison of gradients.

    Args:
        state (DefaultState): State information, configures pre- and post-division factors.
        grad (torch.Tensor): A gradient for the local batch that needs to be communicated across ranks.
    """
    # Average grad by pre-division factor. Together pre- and post-division factors
    # lead to an overall averaging by world_size, required for consistency with PyTorch DDP.
    # This is a two-step process to avoid potential underflow and overflow.
    if state.gradient_predivide_factor > 1:
        grad.div_(state.gradient_predivide_factor)
    dist.all_reduce(grad, group=state.process_group)
    # Average grad by post-division factor.
    if state.gradient_postdivide_factor > 1:
        grad.div_(state.gradient_postdivide_factor)

def reduce_scatter_hook(state: DefaultState, grad: torch.Tensor, output: torch.Tensor):
    """
    Implement ''reduce_scatter'' algorithm for sharded FSDP startegies and pre,post divison of gradients.

    Args:
        state (DefaultState): State information, configures pre- and post-division factors.
        grad (torch.Tensor): An unsharded gradient for the local batch that needs to be
        communicated across ranks.
        output (torch.Tensor): Stores a single shard of the gradient after ``reduce_scatter``.
    """
    # Average grad by pre-division factor.
    if state.gradient_predivide_factor > 1:
        grad.div_(state.gradient_predivide_factor)
    dist.reduce_scatter_tensor(
        output, grad, group=state.process_group
    )
    # Average grad's shard by post-division factor.
    if state.gradient_postdivide_factor > 1:
        output.div_(state.gradient_postdivide_factor)

def _low_precision_hook(prec: torch.dtype, state: LowPrecisionState, grad: torch.Tensor, output: torch.Tensor):
    if grad.dtype != prec:
        grad.data = grad.data.to(prec)
    if output is not None:
        if output.dtype != prec:
            output.data = output.data.to(prec)
        reduce_scatter_hook(state, grad, output)
        _decompress(state, output)
    else:
        allreduce_hook(state, grad)
        _decompress(state, grad)

def fp16_compress_hook(state: LowPrecisionState, grad: torch.Tensor, output: Optional[torch.Tensor] = None):
    """
    Implement a simple gradient compression approach that casts ``grad`` to half-precision floating-point format (``torch.float16``).

    It also averages gradients by ``world_size`` in two steps: first it pre-divides gradients by a
    ``state.gradient_predivide_factor``, and after a communication step (``all_reduce`` or ``reduce_scatter``)
    gradients are averaged by a ``state.gradient_postdivide_factor``.
    Once post-division is done, compressed gradients are casted back to parameters' precision.

    Args:
        state (LowPrecisionState): State information, configures pre- and post-division factors, parameters' precision.
        grad (torch.Tensor): A gradient for the local batch that needs to be communicated across ranks in a lower precision.
        output (torch.Tensor): Stores a single shard of the gradient after ``reduce_scatter``.
    """
    fp16_hook = functools.partial(_low_precision_hook, torch.float16)
    return fp16_hook(state, grad, output)

def bf16_compress_hook(state: LowPrecisionState, grad: torch.Tensor, output: Optional[torch.Tensor] = None):
    """
    Implement a simple gradient compression approach for communicating gradients in lower precision.

    This approach involves casting `grad` to the brain float16 format (`torch.bfloat16`),
    and then averaging gradients by `world_size` in two steps: pre-division by `state.gradient_predivide_factor`,
    followed by post-division after a communication step (`all_reduce` or `reduce_scatter`).
    After post-division, the compressed gradients are cast back to the parameters' precision.

    Args:
        state (LowPrecisionState): State information, configures pre- and post-division factors, parameters' precision.
        grad (torch.Tensor): A gradient for the local batch that needs to be communicated across ranks in a lower precision.
        output (torch.Tensor): Stores a single shard of the gradient after ``reduce_scatter``.
    """
    bf16_hook = functools.partial(_low_precision_hook, torch.bfloat16)
    return bf16_hook(state, grad, output)
