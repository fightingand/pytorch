from typing import Tuple

from .optimizer import Optimizer, _params_t

class Adagrad(Optimizer):
    def __init__(self, params: _params_t, lr: float=..., lr_decay: float=..., weight_decay: float=..., initial_accumulator_value: float=...,  eps: float=...) -> None: ...
