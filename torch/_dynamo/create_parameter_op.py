import torch

import logging

torch_log = logging.getLogger("torch")

doc = """
This is used when dynamo traces torch.nn.Parameter, which normally would not trace properly
with AOTAutograd.  We instead create a placeholder torch.nn.Parameter before the graph, which
becomes a graph arg and has no storage backing it.  At the point in the graph where the parameter
actually should be created we mutate this sacrificial placeholder into it.  This allows gradients
to flow into the parameter as if it were an input to the graph (which is the only thing we are
allowed to compute gradients on).
""".strip()


class TracableCreateParameter(torch.autograd.Function):
    @staticmethod
    def forward(ctx, tensor, placeholder):
        assert not tensor.requires_grad
        # torch_log.warning(f"before: placeholder: {placeholder}")
        # torch_log.warning(f"before: tensor: {tensor}")
        if isinstance(tensor, torch.distributed._tensor.api.DTensor):
            placeholder._local_tensor = tensor._local_tensor
            placeholder._spec = tensor._spec
        else:
            placeholder.set_(tensor)
        # torch_log.warning(f"after: placeholder: {placeholder}")
        # torch_log.warning(f"after: tensor: {tensor}")
        return placeholder

    @staticmethod
    def backward(ctx, grad):
        return None, grad  # grad flows to placeholder


def tracable_create_parameter(tensor, placeholder):
    with torch.set_grad_enabled(placeholder.requires_grad):
        out = TracableCreateParameter.apply(tensor, placeholder)
        out = out.clone()
    return out


def new_parameter_placeholder(size, dtype, device, requires_grad):
    """Create a placeholder to be passed to the above functions"""
    result = torch.nn.Parameter(
        torch.empty(size, dtype=dtype, device=device), requires_grad=requires_grad
    )
    # TODO(jansel): alloc followed by free is inefficient, need a way to allocate an unbacked tensor.
    # Allocating a zero tensor would causes assert failures in autograd.
    result.untyped_storage().resize_(0)
    return result


def new_parameter_placeholder_dtensor(size, dtype, device, requires_grad):  # , device_type, mesh, mesh_dim_names, placements_info):
    """Create a placeholder to be passed to the above functions"""
    data_tensor = torch.empty(size, dtype=dtype, device=device)
    data_tensor.untyped_storage().resize_(0)
    # NOTE(yf225): allocate a placeholder nn.Parameter(DTensor), whose content will get swapped out in TracableCreateParameter.forward
    data_tensor = torch.distributed._tensor.api.DTensor.from_local(
        data_tensor,
        device_mesh=torch.distributed.device_mesh.DeviceMesh(device_type=str(device), mesh=[0]),
        placements=[torch.distributed._tensor.Replicate()],
    )
    result = torch.nn.Parameter(
        data_tensor, requires_grad=requires_grad
    )
    torch_log.warning(f"new_parameter_placeholder_dtensor: result: {result}")
    return result
