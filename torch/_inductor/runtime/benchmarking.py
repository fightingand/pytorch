from random import randint
import time
from functools import cached_property, wraps
from itertools import chain
from statistics import mean, median
from typing import Any, Callable, Dict, List, Tuple, Union
from typing_extensions import Concatenate, ParamSpec, Self, TypeVar

import torch
from torch._dynamo.utils import counters, dynamo_timed


logger = torch._logging.getArtifactLogger(__name__, "benchmarking")


MILLISECONDS_PER_SECOND = 1000

P = ParamSpec("P")
T = TypeVar("T")


def time_and_count(
    fn: Callable[Concatenate[Any, P], T]
) -> Callable[Concatenate[Any, P], T]:
    """Wraps `fn` with `dynamo_timed` context, and increments the appropriate dynamo
    counters.
    """

    @wraps(fn)
    def wrapper(self: Any, *args: P.args, **kwargs: P.kwargs) -> T:
        fn_qual_name = f"{self.__class__.__name__}.{fn.__name__}"
        counters["inductor"][f"benchmarking.{fn_qual_name}"] += 1
        with dynamo_timed(fn_qual_name, log_pt2_compile_event=True):
            return fn(self, *args, **kwargs)

    return wrapper


class Benchmarker:
    def __init__(self: Self) -> None:
        pass

    @time_and_count
    def benchmark(
        self: Self,
        fn: Callable[..., Any],
        fn_args: Tuple[Any, ...],
        fn_kwargs: Dict[str, Any],
        **kwargs: Any,
    ) -> float:
        """Benchmark `fn(*fn_args, *fn_kwargs)` and return the runtime, in milliseconds (the
        actual runtime calculation is dictated by the benchmarking implementation, but may be
        one of [mean, median, minimum, etc.]). Functions as a convenience wrapper around
        device-specific implementations, like `benchmark_cpu` and `benchmark_gpu`. Raises
        `ValueError(...)` if we can't safely infer the device type of `fn`; for example,
        if multiple device types are found in `fn_args` and `fn_kwargs`, or if no device
        types are found.

        Arguments:
        - fn: The function to benchmark.
        - fn_args: The function's arguments.
        - fn_kwargs: The function's kwargs.

        Keyword Arguments:
        - **kwargs: The benchmarking implementation's kwargs.

        Returns:
        - The runtime of `fn(*fn_args, **fn_kwargs)`, in milliseconds.
        """
        inferred_device = None
        for arg_or_kwarg in chain(fn_args, fn_kwargs.values()):
            if not isinstance(arg_or_kwarg, torch.Tensor):
                continue
            if inferred_device is None:
                inferred_device = arg_or_kwarg.device
            elif arg_or_kwarg.device != inferred_device:
                raise ValueError(
                    "Can't safely infer the device type of `fn` with multiple device types in `fn_args` and `fn_kwargs`!"
                )
        if inferred_device is None:
            raise ValueError(
                "Can't safely infer the device type of `fn` with no device types in `fn_args` or `fn_kwargs`! You should be calling `.benchmark_cpu` or `.benchmark_gpu` directly."  # noqa: B950
            )
        _callable = lambda: fn(*fn_args, **fn_kwargs)  # noqa: E731
        if inferred_device == torch.device("cpu"):
            return self.benchmark_cpu(_callable, **kwargs)
        # TODO(nmacchioni): For non-CPU functions we default to using the GPU-specific benchmarking
        # implementation which was written specifically with CUDA devices in mind, we may want to
        # explore alternate implementations for other device types.
        return self.benchmark_gpu(_callable, **kwargs)

    @time_and_count
    def benchmark_cpu(
        self: Self, _callable: Callable[[], Any], warmup: int = 20, rep: int = 100
    ) -> float:
        """Benchmark the CPU callable, `_callable`, and return the median runtime,
        in milliseconds.

        Arguments:
        - _callable: The CPU callable to benchmark.

        Keyword Arguments:
        - warmup: Optionally, the duration, in milliseconds, to run `_callable`
        before benchmarking starts.
        - rep: Optionally, the duration, in milliseconds, to run `_callable`
        during benchmarking.

        Returns:
        - The median runtime of `_callable`, in milliseconds.
        """

        def run_for(ms: int) -> List[float]:
            timings = []
            run_start_t = time.perf_counter()
            while True:
                start_t = time.perf_counter()
                _callable()
                end_t = time.perf_counter()
                timings.append((end_t - start_t) * MILLISECONDS_PER_SECOND)
                if ((end_t - run_start_t) * MILLISECONDS_PER_SECOND) > ms:
                    break
            return timings

        run_for(warmup)
        return median(run_for(rep))

    @time_and_count
    def benchmark_gpu(self: Self, *args: Any, **kwargs: Any) -> float:
        raise NotImplementedError
    
    @time_and_count
    def benchmark_many_cpu(
        self: Self, callables: List[Callable[[], Any]], *args: Any, **kwargs: Any
    ) -> List[float]:
        return [
            self.benchmark_cpu(_callable, *args, **kwargs) for _callable in callables
        ]
    
    @time_and_count
    def benchmark_many_gpu(
        self: Self, callables: List[Callable[[], Any]], *args: Any, **kwargs: Any
    ) -> List[float]:
        return [
            self.benchmark_gpu(_callable, *args, **kwargs) for _callable in callables
        ]
    
    @time_and_count
    def lazy_benchmark(self: Self, *args: Any, **kwargs: Any) -> float:
        return self.benchmark(*args, **kwargs)
    
    @time_and_count
    def lazy_benchmark_cpu(self: Self, *args: Any, **kwargs: Any) -> float:
        return self.benchmark_cpu(*args, **kwargs)
    
    @time_and_count
    def lazy_benchmark_gpu(self: Self, *args: Any, **kwargs: Any) -> float:
        return self.benchmark_gpu(*args, **kwargs)


class TritonBenchmarker(Benchmarker):
    @cached_property
    def triton_do_bench(self: Self) -> Callable[..., Any]:
        """Lazily import Triton's `do_bench`."""
        try:
            from triton.testing import do_bench
        except ImportError as e:
            raise NotImplementedError("requires Triton") from e
        return do_bench

    @time_and_count
    def benchmark_gpu(self: Self, _callable: Callable[[], Any], **kwargs: Any) -> float:
        """Benchmark the GPU callable, `_callable`, and return the runtime, in milliseconds.

        Arguments:
        - _callable: The GPU callable to benchmark.

        Keyword Arguments:
        - quantiles: Optionally, a tuple of floats denoting the requested quantiles.
        - return_mode: Optionally, the requested return mode. Currently, Triton's
        `do_bench` supports min, max, mean, and median return modes.
        - **kwargs: Additional kwargs passed to Triton's `do_bench`.

        Returns:
        - The runtime of `callable`, in milliseconds. If `kwargs["quantiles"]` is specified,
        this is the first requested quantile. Else, if `kwargs["return_mode"]` is specified,
        this is the requested return mode. Otherwise, this is the median.
        """
        if "quantiles" in kwargs:
            return self.triton_do_bench(_callable, **kwargs)[0]
        elif "return_mode" in kwargs:
            return self.triton_do_bench(_callable, **kwargs)
        return self.triton_do_bench(_callable, **kwargs, return_mode="median")


class InductorBenchmarker(TritonBenchmarker):
    @cached_property
    def L2_cache_size(self: Self) -> int:
        """Get the L2 cache size, in bytes, of the current device."""
        device = torch.cuda.current_device()
        props = torch.cuda.get_device_properties(device)
        return props.L2_cache_size

    def get_event_pairs(
        self: Self, iters: int
    ) -> List[Tuple[torch.cuda.Event, torch.cuda.Event]]:
        """Get `iters` pairs of CUDA events."""
        return [
            (
                torch.cuda.Event(enable_timing=True),
                torch.cuda.Event(enable_timing=True),
            )
            for _ in range(iters)
        ]

    def get_event_pairs_min_timing(
        self: Self, event_pairs: List[Tuple[torch.cuda.Event, torch.cuda.Event]]
    ) -> float:
        """Get the minimum timing, in milliseconds, for a group of CUDA event pairs."""
        return min(
            [
                start_event.elapsed_time(end_event)
                for start_event, end_event in event_pairs
            ]
        )

    @time_and_count
    def benchmark_gpu(
        self: Self,
        _callable: Callable[[], Any],
        estimation_iters: int = 5,
        memory_warmup_iters: int = 100,
        benchmark_iters: int = 100,
        max_benchmark_duration: int = 25,
        **kwargs: Any,
    ) -> float:
        """Benchmark a GPU callable using a custom benchmarking implementation.

        Arguments:
        - _callable: The callable to benchmark.

        Keyword Arguments:
        - estimation_iters: Optionally, the number of iterations to run `_callable`
        during runtime estimation.
        - memory_warmup_iters: Optionally, the number of iterations to flush the L2
        cache before starting benchmarking.
        - benchmark_iters: Optionally, the number of iterations to run `_callable`
        during the benchmarking.
        - max_benchmark_duration: Optionally, the maximum duration of the benchmarking,
        in milliseconds. An estimated duration is calculated based on the values
        of `memory_warmup_iters` and `benchmark_iters`, along with the estimated
        runtime of `_callable` and various other factors, and we then shrink
        `benchmark_iters` to fit in the alloted maximum duration.
        - **kwargs: Additional kwargs that may be passed to the fallback.

        Returns:
        - The minimum runtime of `_callable`, in milliseconds.
        """
        # we don't want any outside errors propagating into benchmarking
        torch.cuda.synchronize()

        # warmup `_callable` (and catches any failures in the process)
        _callable()
        torch.cuda.synchronize()

        # see https://github.com/triton-lang/triton/pull/840 for why `dtype=torch.int`
        buffer = torch.empty(self.L2_cache_size // 4, dtype=torch.int, device="cuda")
        buffer.zero_()

        # estimate the runtime of `_callable`
        event_pairs = self.get_event_pairs(estimation_iters)
        for start_event, end_event in event_pairs:
            buffer.zero_()
            start_event.record()
            _callable()
            end_event.record()
        torch.cuda.synchronize()
        estimated_timing = self.get_event_pairs_min_timing(event_pairs)

        # adjust `benchmark_iters` to fit in the maximum benchmarking duration
        benchmark_iters = max(
            min(benchmark_iters, int(max_benchmark_duration // estimated_timing)), 1
        )

        # do the memory warmup
        for _ in range(memory_warmup_iters):
            buffer.zero_()

        # benchmark `_callable`
        event_pairs = self.get_event_pairs(benchmark_iters)
        for start_event, end_event in event_pairs:
            buffer.zero_()
            start_event.record()
            _callable()
            end_event.record()
        torch.cuda.synchronize()
        benchmarked_timing = self.get_event_pairs_min_timing(event_pairs)

        # explicitly delete the buffer, sometimes helps memory
        # footprint metrics in OSS Inductor performance benchmarks
        del buffer

        # return the minimum of `estimated_timing` and `benchmarked_timing`,
        # we just want the minimum timing overall so we might as well check both
        return min(estimated_timing, benchmarked_timing)


class GroupedInductorBenchmarker(InductorBenchmarker):
    def get_interleaved_event_pairs(
        self: Self, num_callables: int, iters: int
    ) -> List[List[Tuple[torch.cuda.Event, torch.cuda.Event]]]:
        """Get `iters` interleaved `num_callables` pairs of CUDA events."""
        return [self.get_event_pairs(num_callables) for _ in range(iters)]

    def get_interleaved_event_pairs_min_timing(
        self: Self,
        interleaved_event_pairs: List[List[Tuple[torch.cuda.Event, torch.cuda.Event]]],
    ) -> List[float]:
        """Get the interleaved minimum timings, in milliseconds, for an interleaved
        grouping of CUDA event pairs.
        """
        return [self.get_event_pairs_min_timing(list(event_pairs)) for event_pairs in zip(*interleaved_event_pairs)]

    @time_and_count
    def benchmark_many_gpu(
        self: Self,
        callables: List[Callable[[], Any]],
        estimation_iters: int = 5,
        memory_warmup_iters: int = 100,
        benchmark_iters: int = 100,
        max_benchmark_duration: int = 25,
        **kwargs: Any,
    ) -> List[float]:
        """Benchmark many GPU callables using a custom benchmarking implementation.

        Arguments:
        - callables: The callables to benchmark.

        Keyword Arguments:
        - estimation_iters: The number of iterations to run `_callable` during
        runtime estimation.
        - memory_warmup_iters: The number of iterations to flush the L2 cache
        before benchmarking.
        - benchmark_iters: The number of iterations to run `_callable` during
        benchmarking.
        - max_benchmark_duration: The maximum duration of the benchmarking per
        callable, in milliseconds. An estimated duration is calculated based on
        the values of `memory_warmup_iters` and `benchmark_iters`, along with the
        estimated runtime of `_callable` and various other factors, and we then
        shrink `benchmark_iters` to fit in the alloted maximum duration.
        - **kwargs: Additional kwargs that may be passed to the fallback.

        Returns:
        - The minimum runtime of each callable in `callables`, in milliseconds.
        """
        # we don't want any outside errors propagating into benchmarking
        torch.cuda.synchronize()

        # warmup each callable in `callables` (and catches any failures in the process)
        for _callable in callables:
            _callable()
        torch.cuda.synchronize()

        # see https://github.com/triton-lang/triton/pull/840 for why `dtype=torch.int`
        buffer = torch.empty(self.L2_cache_size // 4, dtype=torch.int, device="cuda")
        buffer.zero_()

        # estimate the runtime of `_callable`
        interleaved_event_pairs = self.get_interleaved_event_pairs(
            len(callables), estimation_iters
        )
        for event_pairs in interleaved_event_pairs:
            for _callable, (start_event, end_event) in zip(callables, event_pairs):
                buffer.zero_()
                start_event.record()
                _callable()
                end_event.record()
        torch.cuda.synchronize()
        estimated_timings = self.get_interleaved_event_pairs_min_timing(
            interleaved_event_pairs
        )

        # adjust `benchmark_iters` to fit in the maximum benchmarking duration, we're
        # alloted `max_benchmark_duration` per-callable, so we can just take the average
        # of the estimated timings
        benchmark_iters = max(
            min(benchmark_iters, max_benchmark_duration // mean(estimated_timings)), 1
        )

        # do the memory warmup
        for _ in range(memory_warmup_iters):
            buffer.zero_()

        # benchmark `_callable`
        interleaved_event_pairs = self.get_interleaved_event_pairs(
            len(callables), estimation_iters
        )
        for event_pairs in interleaved_event_pairs:
            for _callable, (start_event, end_event) in zip(callables, event_pairs):
                buffer.zero_()
                start_event.record()
                _callable()
                end_event.record()
        torch.cuda.synchronize()
        benchmarked_timings = self.get_interleaved_event_pairs_min_timing(
            interleaved_event_pairs
        )

        # explicitly delete the buffer, sometimes helps memory
        # footprint metrics in OSS Inductor performance benchmarks
        del buffer

        # return the minimum of estimated_timing and benchmarked_timing, since
        # we just want the minimum timing overall we might check both
        return [
            min(estimated_timing, benchmarked_timing)
            for estimated_timing, benchmarked_timing in zip(
                estimated_timings, benchmarked_timings
            )
        ]


class LazyBenchmark:
    def __init__(self: Self, benchmark: Callable[[], float]) -> None:
        self.benchmark = benchmark

    @cached_property
    @time_and_count
    def timing_ms(self: Self) -> float:
        counters["inductor"]["benchmarking_finalize_lazy_benchmark"] += 1
        timing_ms = self.benchmark()
        # I don't think this helps with saving memory at all,
        # but at least it gives good signal if we ever try
        # to call self.benchmark again
        del self.benchmark
        return timing_ms

    def __float__(self: Self) -> float:
        return float(self.timing_ms)


class LazyInductorBenchmarker(GroupedInductorBenchmarker):
    def __init__(self: Self) -> None:
        self.memory_cache: Dict[str, float] = {}
        self.kwargs_hash_to_futures_gpu: Dict[
            str, List[Tuple[Callable[[], Any], str]]
        ] = {}

    @time_and_count
    def lazy_benchmark(
        self: Self,
        fn: Callable[..., Any],
        fn_args: Tuple[Any],
        fn_kwargs: Dict[str, Any],
        **kwargs: Any,
    ) -> Union[LazyBenchmark, float]:
        """Lazily enque `fn(*fn_args, *fn_kwargs)` for benchmarking. Follows the same
        principles as `Benchmarking.benchmark` except that the benchmarking is delayed,
        in the form of returning a `LazyBenchmark` object, such that benchmarking does
        not occur until the benchmarking results are required. This allows us to have
        better benchmark grouping (i.e. we can utilize `GroupedInductorBenchmarker`)
        without having to refactor user callsites.

        Arguments:
        - fn: The function to benchmark.
        - fn_args: The function's arguments.
        - fn_kwargs: The function's kwargs.

        Keyword Arguments:
        - **kwargs: The benchmarking implementation's kwargs.

        Returns:
        - A `LazyBenchmark`, `x`, that evaluates to the actual benchmark result
        when `float(x)` is evaluated, or the actual runtime of `fn(*fn_args, **fn_kwargs)`,
        in milliseconds, if the device type is `"cpu"` (since lazy benchmarking is only
        implemented for `"gpu"` device types).
        """
        inferred_device = None
        for arg_or_kwarg in chain(fn_args, fn_kwargs.values()):
            if not isinstance(arg_or_kwarg, torch.Tensor):
                continue
            if inferred_device is None:
                inferred_device = arg_or_kwarg.device
            elif arg_or_kwarg.device != inferred_device:
                raise ValueError(
                    "Can't safely infer the device type of `fn` with multiple device types in `fn_args` and `fn_kwargs`!"
                )
        if inferred_device is None:
            raise ValueError(
                "Can't safely infer the device type of `fn` with no device types in `fn_args` or `fn_kwargs`! You should be calling `.benchmark_cpu` or `.benchmark_gpu` directly."  # noqa: B950
            )
        _callable = lambda: fn(*fn_args, **fn_kwargs)  # noqa: E731
        if inferred_device == torch.device("cpu"):
            return self.lazy_benchmark_cpu(_callable, **kwargs)
        return self.lazy_benchmark_gpu(_callable, **kwargs)
    
    @time_and_count
    def lazy_benchmark_gpu(
        self: Self,
        _callable: Callable[[], Any],
        **kwargs: Any,
    ) -> Union[LazyBenchmark, float]:
        # we should try the callable before queueing it for benchmarking, in
        # case it throws an exception. we could catch and handle any exception
        # later on, but some codepaths expect and handle certain exceptions
        _callable()
        torch.cuda.synchronize()

        # we want to group benchmarks based on the kwargs hash, this handles
        # grouping benchmarks by ranking keys and pruning keys, and also ensures
        # that we only benchmark callables that should run under the same conditions
        # with respect to warmup, benchmarking, etc.
        kwargs_hash = str(hash(tuple(sorted(kwargs.items()))))
        # we've seen that just hash(_callable) and the kwargs_hash are not enough to
        # differentiate callables; if _callable is something like a lambda, which then
        # goes out of scope and gets garbage collected, its memory address may be later
        # reused for a different _callable. if this is the case, the latter _callable would
        # incorrectly exist in the memory cache, which could lead to memory leaks if we
        # have a lazy benchmark grouping of one, because we would never remove _callable
        # from the lazy benchmark queue and as such any memory referenced by _callable
        # would remain allocated
        key = str(hash(_callable) + randint(-(2**100), 2**100)) + kwargs_hash
        self.kwargs_hash_to_futures_gpu.setdefault(kwargs_hash, []).append(
            (_callable, key)
        )

        def benchmark() -> float:
            # all but the first benchmark in a grouping of lazy benchmarks
            # should be cached in memory, so we should return that cached timing
            if key in self.memory_cache:
                return self.memory_cache[key]

            futures_gpu = self.kwargs_hash_to_futures_gpu.pop(kwargs_hash)
            callables, keys = zip(*futures_gpu)
            callables, keys = list(callables), list(keys)

            try:
                timings_ms = self.benchmark_many_gpu(callables, **kwargs)
            except Exception as e:  # noqa: TRY302
                raise e
            else:
                self.memory_cache.update(zip(keys, timings_ms))
                return self.memory_cache[key]
            finally:
                # we have seen cases where not explicitly deleting the GPU futures
                # can prevent the memory allocated for the callables from being
                # properly and timely cleaned up, which can have fatal interactions
                # in cudagraphs mode
                del futures_gpu

        return LazyBenchmark(benchmark)


benchmarker = LazyInductorBenchmarker()
