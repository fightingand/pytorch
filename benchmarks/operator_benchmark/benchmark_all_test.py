import benchmark_all_other_test  # noqa
import benchmark_all_quantized_test  # noqa
import operator_benchmark as op_bench
from pt import unary_test  # noqa; noqa

if __name__ == "__main__":
    op_bench.benchmark_runner.main()
