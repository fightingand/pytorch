import torch
from torch.nn.functional import Reduction
from .Criterion import Criterion


class DistKLDivCriterion(Criterion):

    def __init__(self, sizeAverage=True):
        super(DistKLDivCriterion, self).__init__()
        self.sizeAverage = sizeAverage
        self.output_tensor = torch.Tensor(1)

    def updateOutput(self, input, target):
        assert input.is_same_size(target)
        if self.output_tensor is None:
            self.output_tensor = input.new(1)
        self._backend.DistKLDivCriterion_updateOutput(
            self._backend.library_state,
            input,
            target,
            self.output_tensor,
            Reduction.legacy_get_enum(self.sizeAverage, True),
        )
        self.output = self.output_tensor[0].item()
        return self.output

    def updateGradInput(self, input, target):
        assert input.is_same_size(target)
        implicit_gradOutput = torch.ones(1).type_as(input)
        self._backend.DistKLDivCriterion_updateGradInput(
            self._backend.library_state,
            input,
            target,
            implicit_gradOutput,
            self.gradInput,
            Reduction.legacy_get_enum(self.sizeAverage, True),
        )
        return self.gradInput
