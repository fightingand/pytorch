#include "THCUNN.h"
#include "THCTensor.hpp"
#include "common.h"
#include "im2col.h"

#include "THCHalf.h"
#include "THCHalfAutoNumerics.cuh"

#include "generic/SpatialConvolutionMM.cu"
#include "THCGenerateFloatTypes.h"
