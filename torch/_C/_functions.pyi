from typing import AnyStr, List

from torch import Tensor

class UndefinedGrad:
    def __init__(self) -> None: ...
    def __call__(self, *inputs: Tensor) -> List[Tensor]: ...
    ...

class DelayedError:
    def __init__(self, msg: AnyStr, num_inputs: int) -> None: ...
    def __call__(self, inputs: List[Tensor]) -> List[Tensor]: ...
    ...
