import logging
import warnings
import weakref
import torch
import torch.distributed as dist
import torch.distributed.distributed_c10d as c10d
from typing import List, cast

"""
Moved eager kernel implementations to a separate file partly for readability and partly as it is currently
easier in dynamo to set tracing policy on a file-by-file level.

Do not put code in this file that Dynamo is expected to trace into, as dynamo may disallow this whole file.
"""

logger = logging.getLogger(__name__)

data_ptr_to_work = dict()
work_version = 0

class _WaitRegistration:
    def __init__(self, work):
        global work_version
        self.work = work
        self.version = work_version
        self.ptrs = []
        self.cleanup_count = 0
        work_version += 1

    def _register(self, tensor):
        global data_ptr_to_work
        ptr = tensor.data_ptr()
        data_ptr_to_work[ptr] = self
        self.ptrs.append(ptr)
        self.cleanup_count += 1

    def wait(self):
        if self.work is not None:
            self.work.wait()
            self.work = None
        self.cleanup()

    def decrement_live_tensor(self, ptr):
        self.cleanup_count -= 1
        if self.cleanup_count == 0:
            self.cleanup()
        else:
            if data_ptr_to_work.get(ptr, None) == self:
                del data_ptr_to_work[ptr]

    def cleanup(self):
        for ptr in self.ptrs:
            if data_ptr_to_work.get(ptr, None) == self:
                del data_ptr_to_work[ptr]


def _register_tensor_work(tensor_or_list, work_or_list):
    if not isinstance(tensor_or_list, list):
        tensor_or_list = [tensor_or_list]
    if not isinstance(work_or_list, list):
        reg = _WaitRegistration(work_or_list)
        for tensor in tensor_or_list:
            reg._register(tensor)
    else:
        for tensor, work in zip(tensor_or_list, work_or_list):
            reg = _WaitRegistration(work)
            reg._register(tensor)

def _wait_reg_dec(ptr, wait_reg):
    wait_reg.decrement_live_tensor(ptr)

def _register_wrapper_tensor(tensor_wrapper, tensor):
    global data_ptr_to_work
    # Note: we should NEVER try to trace this, bc it registers runtime stuff during trace.
    # Instead, backends must call this themselves when implementing traced collectives.
    wait_reg = data_ptr_to_work.get(tensor.data_ptr(), None)
    if wait_reg is None:
        warnings.warn(
            "Trying to register finalizers to AsyncCollectiveTensor but the inner tensor is already gone"
        )
    else:
        # We force the collective to be waited in the case this tensor goes away to reduce the change of deadlocks.
        weakref.finalize(tensor_wrapper, _wait_reg_dec, tensor.data_ptr(), wait_reg)

def _wait_tensor(tensor: torch.Tensor) -> torch.Tensor:
    global data_ptr_to_work
    data_ptr = tensor.data_ptr()
    wait_reg = data_ptr_to_work.get(data_ptr)
    if wait_reg is not None:
        wait_reg.wait()
    return tensor

def _str_to_reduce_op(reduceOp: str) -> dist.ReduceOp:
    reduceOp = reduceOp.upper()
    op = dist.ReduceOp.RedOpType.__members__.get(reduceOp)
    if op is None:
        raise ValueError(f"Invalid reduce operation {reduceOp}")
    return cast(dist.ReduceOp, op)


"""
Kernel implementations (for eager runtime only) - should never be traced by torch.compile

These functions should all be bound to dispatcher ops.  During tracing, the op itself should be
captured in the graph and the backend should implement the op however it prefers.
"""
# TODO assert if ranks has duplicated entries
def _all_reduce(self, reduceOp, tag, ranks, group_size):
    op = _str_to_reduce_op(reduceOp)
    group = c10d._find_or_create_pg_by_ranks_and_tag(tag, ranks, group_size)
    assert group is not None

    inplace_tensor = self.clone(memory_format=torch.contiguous_format)
    work = dist.all_reduce(inplace_tensor, op=op, group=group, async_op=True)
    _register_tensor_work(inplace_tensor, work)

    return inplace_tensor

def _all_reduce_coalesced(self, reduceOp, tag, ranks, group_size):
    op = _str_to_reduce_op(reduceOp)
    group = c10d._find_or_create_pg_by_ranks_and_tag(tag, ranks, group_size)
    assert group is not None

    inplace_tensor_list = [t.clone(memory_format=torch.contiguous_format) for t in self]
    work = dist.all_reduce_coalesced(inplace_tensor_list, op=op, group=group, async_op=True)
    _register_tensor_work(inplace_tensor_list, work)

    return inplace_tensor_list

def _all_gather_into_tensor(shard, tag, ranks, group_size):
    # TODO add dim support?
    group = c10d._find_or_create_pg_by_ranks_and_tag(tag, ranks, group_size)
    assert group is not None
    out_size = list(shard.size())
    out_size[0] *= group_size
    out_tensor = shard.new_empty(out_size)
    assert out_tensor.is_contiguous()
    # FIXME gloo doesn't support _allgather_base
    if dist.get_backend(group) == dist.Backend.GLOO or shard.is_cpu:
        tensor_list = list(torch.chunk(out_tensor, group_size))
        work = dist.all_gather(tensor_list, shard, group=group, async_op=True)
    else:
        work = dist.all_gather_into_tensor(out_tensor, shard, group=group, async_op=True)
    _register_tensor_work(out_tensor, work)

    return out_tensor


def _all_gather_into_tensor_coalesced(self, tag, rankset, group_size):
    group = c10d._find_or_create_pg_by_ranks_and_tag(tag, rankset, group_size)
    assert group is not None

    def mk_out_tensor(shard):
        out_size = list(shard.size())
        out_size[0] *= group_size
        out_tensor = shard.new_empty(out_size)
        assert out_tensor.is_contiguous()
        return out_tensor

    out_tensors = [mk_out_tensor(t) for t in self]

    work_list = _all_gather_into_tensor_coalesced_fallback(
        output_tensors=out_tensors,
        input_tensors=self,
        group=group,
        async_op=True)


    _register_tensor_work(out_tensors, work_list)
    return out_tensors

def _reduce_scatter_tensor(
    input: torch.Tensor,
    reduceOp: str,
    tag: str,
    ranks: List[int],
    group_size: int,
):
    # TODO add dim support?
    group = c10d._find_or_create_pg_by_ranks_and_tag(tag, ranks, group_size)
    assert group is not None
    op = _str_to_reduce_op(reduceOp)

    if dist.get_backend(group) == dist.Backend.GLOO or input.is_cpu:
        # cpu::gloo backend does not have reduce_scatter we fallback to do all_reduce
        # + local chunk
        logger.warning(
            "ProcessGroupGloo does not support reduce_scatter, falling back with all reduce!"
        )
        reduction_input = input.clone()
        group_rank = dist.get_rank(group)
        work = dist.all_reduce(reduction_input, op=op, group=group, async_op=True)
        out_tensor = reduction_input.chunk(group_size, dim=0)[group_rank]
        _register_tensor_work(out_tensor, work)
    else:
        out_size = list(input.size())
        out_size[0] //= group_size
        out_tensor = input.new_empty(out_size)
        work = dist.reduce_scatter_tensor(
            out_tensor, input, op=op, group=group, async_op=True
        )
        _register_tensor_work(out_tensor, work)

    return out_tensor

def _reduce_scatter_tensor_coalesced(
    inputs: List[torch.Tensor],
    reduce_op: str,
    tag: str,
    ranks: List[int],
    group_size: int,
):
    group = c10d._find_or_create_pg_by_ranks_and_tag(tag, ranks, group_size)
    assert group is not None
    op = _str_to_reduce_op(reduce_op)


    def mk_out_tensor(shard):
        out_size = list(shard.size())
        out_size[0] //= group_size
        out_tensor = shard.new_empty(out_size)
        assert out_tensor.is_contiguous()
        return out_tensor

    out_tensors = [mk_out_tensor(t) for t in inputs]

    work_list = _reduce_scatter_tensor_coalesced_fallback(
        output_tensors=out_tensors,
        input_tensors=inputs,
        op=op,
        group=group,
        async_op=False)

    _register_tensor_work(out_tensors, work_list)
    return out_tensors

def _all_gather_into_tensor_coalesced_fallback(output_tensors, input_tensors, group, async_op=False):
    # all_gather_coalesced is useless, it doesn't work under NCCL and does lots of copies under Gloo
    # all_gather is useless too because it's single tensor
    # NCCL's PG::all_gather with multiple tensors is broken, it only works for the multi-device setting
    #  and fails if you mix same-size with different-size tensor lists.
    # _coalescing_manager crashed NCCL when used with all_gather_into_tensor.
    if input_tensors[0].is_cpu or not async_op:
        work_list = []
        out_tensors_sliced = [
            list(torch.chunk(out_tensor, dist.get_world_size(group)))
            for out_tensor in output_tensors
        ]
        for shard, out_tensor in zip(input_tensors, out_tensors_sliced):
            work = c10d.all_gather(out_tensor, shard, group=group, async_op=async_op)
            work_list.append(work)
        return work_list
    else:
        with c10d._coalescing_manager(group=group, async_ops=True) as cm:
            for in_t, out_t in zip(input_tensors, output_tensors):
                dist.all_gather_into_tensor(out_t, in_t, group=group, async_op=True)
        return cm

def _reduce_scatter_tensor_coalesced_fallback(output_tensors, input_tensors, op, group, async_op=False):
    # All the same reasons as the all_gather fallback
    work_list = []
    for shard, out_tensor in zip(input_tensors, output_tensors):
        work = c10d.reduce_scatter_tensor(out_tensor, shard, op=op, group=group, async_op=async_op)
        work_list.append(work)
    return work_list
