#ifndef TH_GENERIC_FILE
#define TH_GENERIC_FILE "THNN/generic/GatedLinearUnit.c"
#else

#include <ATen/WrapDimUtils.h>

void THNN_(GatedLinear_updateOutput)(
          THNNState *state,
          THTensor *input,
          THTensor *output,
          int dim)
{
  dim = at::maybe_wrap_dim(dim, input);
  // size output to half of input
  const int64_t nIn = THTensor_sizeLegacyNoScalars(input, dim);
  THArgCheck(nIn % 2 == 0, 2, "Halving dimension must be even. Dim %d is size %ld",
      dim, nIn);

  const int64_t inputSize = THTensor_(size)(input, dim) / 2;
  std::vector<int64_t> newSizes = THTensor_sizesLegacyNoScalars(input);
  newSizes[dim] = inputSize;
  THTensor_(resize)(output, newSizes, {});

  // halve tensor
  THTensor *firstHalf = THTensor_(newNarrow)(input, dim, 0, inputSize);
  THTensor *secondHalf = THTensor_(newNarrow)(input, dim, inputSize, inputSize);

  // x = x1:cmul( sigmoid(x2) )
  at::Tensor output_wrap = THTensor_wrap(output);
  at::Tensor secondHalf_wrap = THTensor_wrap(secondHalf);
  at::native::sigmoid_out(output_wrap, secondHalf_wrap);
  THTensor_(cmul)(output, output, firstHalf);

  c10::raw::intrusive_ptr::decref(firstHalf);
  c10::raw::intrusive_ptr::decref(secondHalf);
}

void THNN_(GatedLinear_updateGradInput)(
          THNNState *state,
          THTensor *input,
          THTensor *gradOutput,
          THTensor *gradInput,
          int dim)
{
  dim = at::maybe_wrap_dim(dim, input);
  // set up tensors
  const int64_t nIn = THTensor_(size)(input, dim);
  THArgCheck(nIn % 2 == 0, 2, "Halving dimension must be even. Dim %d is size %ld",
      dim, nIn);

  THTensor_(resizeAs)(gradInput, input);
  const int64_t inputSize = THTensor_(size)(input, dim) / 2;
  THTensor *firstHalf = THTensor_(newNarrow)(input, dim, 0, inputSize);
  THTensor *secondHalf = THTensor_(newNarrow)(input, dim, inputSize, inputSize);
  THTensor *gradInputfirstHalf = THTensor_(newNarrow)(gradInput, dim, 0, inputSize);
  THTensor *gradInputsecondHalf = THTensor_(newNarrow)(gradInput, dim, inputSize, inputSize);

  at::Tensor gradInputfirstHalf_wrap = THTensor_wrap(gradInputfirstHalf);
  at::Tensor secondHalf_wrap = THTensor_wrap(secondHalf);
  at::native::sigmoid_out(gradInputfirstHalf_wrap, secondHalf_wrap);

  TH_TENSOR_APPLY2(scalar_t, gradInputsecondHalf, scalar_t, gradInputfirstHalf,
    scalar_t z = *gradInputfirstHalf_data;
    *gradInputsecondHalf_data = (1. - z) * z;
  );

  THTensor_(cmul)(gradInputfirstHalf, gradInputfirstHalf, gradOutput);

  THTensor_(cmul)(gradInputsecondHalf, gradInputsecondHalf, gradOutput);
  THTensor_(cmul)(gradInputsecondHalf, gradInputsecondHalf, firstHalf);

  c10::raw::intrusive_ptr::decref(firstHalf);
  c10::raw::intrusive_ptr::decref(secondHalf);
  c10::raw::intrusive_ptr::decref(gradInputfirstHalf);
  c10::raw::intrusive_ptr::decref(gradInputsecondHalf);
}

#endif
