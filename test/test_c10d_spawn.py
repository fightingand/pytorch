import tempfile
import unittest

import torch
import torch.distributed as c10d
import torch.multiprocessing as mp

from common_cuda import TEST_MULTIGPU
from common_utils import TestCase, load_tests, run_tests
from common_utils import NO_MULTIPROCESSING_SPAWN

# load_tests from common_utils is used to automatically filter tests for
# sharding on sandcastle. This line silences flake warnings
load_tests = load_tests

if not c10d.is_available():
    print('c10d not available, skipping tests')
    sys.exit(0)


if NO_MULTIPROCESSING_SPAWN:
    print('spawn not available, skipping tests')
    sys.exit(0)


NO_NCCL = not hasattr(c10d, "ProcessGroupNCCL")


class ProcessGroupShareTensorTest(TestCase):

    @property
    def world_size(self):
        return 2

    @classmethod
    def opts(cls, threads=2):
        opts = c10d.ProcessGroupGloo.Options()
        opts.devices = [c10d.ProcessGroupGloo.create_tcp_device(interface="lo")]
        opts.timeout = 5.0
        opts.threads = threads
        return opts

    @classmethod
    def _init_pg_gloo(cls, rank, filename, world_size):
        store = c10d.FileStore(filename, world_size)
        return c10d.ProcessGroupGloo(
            store, rank, world_size, ProcessGroupShareTensorTest.opts())

    @classmethod
    def _init_pg_nccl(cls, rank, filename, world_size):
        store = c10d.FileStore(filename, world_size)
        return c10d.ProcessGroupNCCL(store, rank, world_size)

    @classmethod
    def assert_equal(cls, expected, value):
        assert (expected == value).all().item() == 1, (
            "Expecting tensor value {} but got {}."
        ).format(expected, value)

    def _test_multiprocess(self, f, shared_tensors, init_pg, n_output):
        ws = self.world_size
        with tempfile.NamedTemporaryFile(delete=False) as file:
            ctx = mp.get_context('spawn')
            c2p = ctx.Queue(2)
            p2c = ctx.Queue(2)
            ps = []
            for i in range(ws):
                p = ctx.Process(
                    target=f,
                    args=(i, file.name, shared_tensors, ws, init_pg, c2p, p2c))

                p.start()
                ps.append(p)

            for _ in range(ws * n_output):
                pid, expected, result = c2p.get()
                self.assertEqual(expected, result, (
                    "Expect rank {} to broadcast result {} but got {}."
                    ).format(pid, expected, result))

            for _ in range(ws):
                p2c.put(0)

            for p in ps:
                p.join()

    # Why classmethod? multiprocessing cannot pickle TestCase subclass when in
    # spawn mode. See https://bugs.python.org/issue33884.
    @classmethod
    def _test_broadcast_process(
            cls, rank, filename, shared_tensors, world_size, init_pg, c2p, p2c):
        pg = init_pg(rank, filename, world_size)
        xs = [shared_tensors[rank]]
        pg.broadcast(xs).wait()
        c2p.put((rank, torch.zeros(2, 2) + 1, xs[0].to("cpu")))
        p2c.get()

    @unittest.skipIf(not TEST_MULTIGPU, "At least 2 CUDA GPUS needed")
    def test_shared_broadcast_gloo(self):
        self._test_multiprocess(
            ProcessGroupShareTensorTest._test_broadcast_process,
            [torch.ones(2, 2).to(i) * i for i in range(self.world_size)],
            ProcessGroupShareTensorTest._init_pg_gloo,
            1)


    @unittest.skipIf(not TEST_MULTIGPU, "At least 2 CUDA GPUS needed")
    @unittest.skipIf(NO_NCCL, "NCCL needed")
    def test_shared_broadcast_nccl(self):
        self._test_multiprocess(
            ProcessGroupShareTensorTest._test_broadcast_process,
            [torch.ones(2, 2).to(i) * i for i in range(self.world_size)],
            ProcessGroupShareTensorTest._init_pg_nccl,
            1)

    @classmethod
    def _test_allreduce_process(
            cls, rank, filename, shared_tensors, world_size, init_pg):
        pg = init_pg(rank, filename, world_size)
        xs = [shared_tensors[rank]]
        pg.allreduce(xs, op=c10d.ReduceOp.SUM).wait()
        # deliberate failure to try test suite output
        #cls.assert_equal(torch.ones(2, 2) * 2, xs[0].to("cpu"))
        cls.assert_equal(torch.ones(2, 2) * 10, xs[0].to("cpu"))

    @unittest.skipIf(not TEST_MULTIGPU, "At least 2 CUDA GPUS needed")
    def test_shared_allreduce_gloo(self):
        with tempfile.NamedTemporaryFile(delete=False) as file:
            shared_tensors = [torch.ones(2, 2).to(i) for i in range(2)]
            mp.spawn(ProcessGroupShareTensorTest._test_allreduce_process,
                     args=(file.name,
                           shared_tensors,
                           self.world_size,
                           ProcessGroupShareTensorTest._init_pg_gloo),
                     nprocs=self.world_size,
                     join=True)

    @unittest.skipIf(not TEST_MULTIGPU, "At least 2 CUDA GPUS needed")
    @unittest.skipIf(NO_NCCL, "NCCL needed")
    def test_shared_allreduce_nccl(self):
        with tempfile.NamedTemporaryFile(delete=False) as file:
            shared_tensors = [torch.ones(2, 2).to(i) for i in range(2)]
            mp.spawn(ProcessGroupShareTensorTest._test_allreduce_process,
                     args=(file.name,
                           shared_tensors,
                           self.world_size,
                           ProcessGroupShareTensorTest._init_pg_nccl),
                     nprocs=self.world_size,
                     join=True)

    @classmethod
    def _test_reduce_process(
            cls, rank, filename, shared_tensors, world_size, init_pg):
        pg = init_pg(rank, filename, world_size)
        x = shared_tensors[rank]
        pg.reduce(x, root=0, op=c10d.ReduceOp.SUM).wait()
        if rank == 0:
            cls.assert_equal(torch.ones(2, 2) * 2, x.to("cpu"))
        else:
            cls.assert_equal(torch.ones(2, 2), x.to("cpu"))

    @unittest.skipIf(not TEST_MULTIGPU, "At least 2 CUDA GPUS needed")
    @unittest.skipIf(NO_NCCL, "NCCL needed")
    def test_shared_reduce_nccl(self):
        with tempfile.NamedTemporaryFile(delete=False) as file:
            shared_tensors = [torch.ones(2, 2).to(i) for i in range(2)]
            mp.spawn(ProcessGroupShareTensorTest._test_reduce_process,
                     args=(file.name,
                           shared_tensors,
                           self.world_size,
                           ProcessGroupShareTensorTest._init_pg_nccl),
                     nprocs=self.world_size,
                     join=True)

    @classmethod
    def _test_allgather_process(
            cls, rank, filename, shared_tensors, world_size, init_pg):
        pg = init_pg(rank, filename, world_size)
        xs = [shared_tensors[rank]]
        ys = [[torch.zeros_like(xs[0]) for i in range(world_size)]]
        pg.allgather(ys, xs).wait()
        for i in range(world_size):
            cls.assert_equal(torch.ones(2, 2) * i, ys[0][i].to("cpu"))

    @unittest.skipIf(not TEST_MULTIGPU, "At least 2 CUDA GPUS needed")
    def test_shared_allgather_gloo(self):
        with tempfile.NamedTemporaryFile(delete=False) as file:
            shared_tensors = [torch.ones(2, 2).to(i) * i for i in range(2)]
            mp.spawn(ProcessGroupShareTensorTest._test_allgather_process,
                     args=(file.name,
                           shared_tensors,
                           self.world_size,
                           ProcessGroupShareTensorTest._init_pg_gloo),
                     nprocs=self.world_size,
                     join=True)

    @unittest.skipIf(not TEST_MULTIGPU, "At least 2 CUDA GPUS needed")
    @unittest.skipIf(NO_NCCL, "NCCL needed")
    def test_shared_allgather_nccl(self):
        with tempfile.NamedTemporaryFile(delete=False) as file:
            shared_tensors = [torch.ones(2, 2).to(i) * i for i in range(2)]
            mp.spawn(ProcessGroupShareTensorTest._test_allgather_process,
                     args=(file.name,
                           shared_tensors,
                           self.world_size,
                           ProcessGroupShareTensorTest._init_pg_nccl),
                     nprocs=self.world_size,
                     join=True)


if __name__ == '__main__':
    run_tests()
