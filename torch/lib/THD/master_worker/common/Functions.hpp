#pragma once

#include <cstdint>

namespace thd {

enum Functions: std::uint16_t {
  // generator functions
  generatorNew,
  generatorCopy,
  generatorFree,
  generatorSeed,
  generatorManualSeed,

  tensorCopyFromMaster,
  tensorCopyFromWorker,

  tensorNew,
  tensorNewWithTensor,
  tensorNewWithSize,
  tensorNewWithSize1d,
  tensorNewWithSize2d,
  tensorNewWithSize3d,
  tensorNewWithSize4d,
  tensorNewWithStorage,
  tensorNewWithStorage1d,
  tensorNewWithStorage2d,
  tensorNewWithStorage3d,
  tensorNewWithStorage4d,
  tensorNewClone,
  tensorNewContiguous,
  tensorNewSelect,
  tensorNewNarrow,
  tensorNewTranspose,
  tensorNewUnfold,
  tensorFree,
  tensorResize,
  tensorResizeAs,
  tensorResize1d,
  tensorResize2d,
  tensorResize3d,
  tensorResize4d,
  tensorResize5d,
  tensorSet,
  tensorSetStorage,
  tensorSetStorage1d,
  tensorSetStorage2d,
  tensorSetStorage3d,
  tensorSetStorage4d,
  tensorNarrow,
  tensorSelect,
  tensorTranspose,
  tensorUnfold,
  tensorSqueeze,
  tensorSqueeze1d,
  tensorNElement,

  tensorGesv,
  tensorTrtrs,
  tensorGels,
  tensorSyev,
  tensorGeev,
  tensorGesvd,
  tensorGesvd2,
  tensorGetri,
  tensorPotrf,
  tensorPotrs,
  tensorPotri,
  tensorQr,
  tensorGeqrf,
  tensorOrgqr,
  tensorOrmqr,
  tensorPstrf,

  tensorFill,
  tensorMaskedFill,
  tensorMaskedCopy,
  tensorMaskedSelect,
  tensorNonzero,
  tensorIndexSelect,
  tensorIndexCopy,
  tensorIndexAdd,
  tensorIndexFill,
  tensorGather,
  tensorScatter,
  tensorScatterFill,
  tensorDot,
  tensorMinall,
  tensorMaxall,
  tensorMedianall,
  tensorSumall,
  tensorProdall,
  tensorNeg,
  tensorCinv,
  tensorAdd,
  tensorSub,
  tensorMul,
  tensorDiv,
  tensorFmod,
  tensorRemainder,
  tensorClamp,
  tensorCadd,
  tensorCsub,
  tensorCmul,
  tensorCpow,
  tensorCdiv,
  tensorCfmod,
  tensorCremainder,
  tensorAddcmul,
  tensorAddcdiv,
  tensorAddmv,
  tensorAddmm,
  tensorAddr,
  tensorAddbmm,
  tensorBaddbmm,
  tensorMatch,
  tensorNumel,
  tensorMax,
  tensorMin,
  tensorKthvalue,
  tensorMode,
  tensorMedian,
  tensorSum,
  tensorProd,
  tensorCumsum,
  tensorCumprod,
  tensorSign,
  tensorTrace,
  tensorCross,
  tensorCmax,
  tensorCmin,
  tensorCmaxValue,
  tensorCminValue,
  tensorDiag,
  tensorEye,
  tensorRange,
  tensorRandperm,
  tensorReshape,
  tensorSort,
  tensorTopk,
  tensorTril,
  tensorTriu,
  tensorCatArray,
  tensorEqual,
  tensorLtValue,
  tensorLeValue,
  tensorGtValue,
  tensorGeValue,
  tensorNeValue,
  tensorEqValue,
  tensorLtValueT,
  tensorLeValueT,
  tensorGtValueT,
  tensorGeValueT,
  tensorNeValueT,
  tensorEqValueT,
  tensorLtTensor,
  tensorLeTensor,
  tensorGtTensor,
  tensorGeTensor,
  tensorNeTensor,
  tensorEqTensor,
  tensorLtTensorT,
  tensorLeTensorT,
  tensorGtTensorT,
  tensorGeTensorT,
  tensorNeTensorT,
  tensorEqTensorT,
  tensorAbs,
  tensorSigmoid,
  tensorLog,
  tensorLog1p,
  tensorExp,
  tensorExpm1,
  tensorCos,
  tensorAcos,
  tensorCosh,
  tensorSin,
  tensorAsin,
  tensorSinh,
  tensorTan,
  tensorAtan,
  tensorAtan2,
  tensorTanh,
  tensorPow,
  tensorTpow,
  tensorSqrt,
  tensorRsqrt,
  tensorCeil,
  tensorFloor,
  tensorRound,
  tensorTrunc,
  tensorFrac,
  tensorLerp,
  tensorMean,
  tensorStd,
  tensorVar,
  tensorNorm,
  tensorRenorm,
  tensorDist,
  tensorHistc,
  tensorBhistc,
  tensorMeanall,
  tensorVarall,
  tensorStdall,
  tensorNormall,
  tensorLinspace,
  tensorLogspace,
  tensorRand,
  tensorRandn,
  tensorLogicalallall,
  tensorLogicalall,
  tensorLogicalanyall,
  tensorLogicalany,

  // th_random
  tensorRandom,
  tensorGeometric,
  tensorBernoulli,
  tensorBernoulli_FloatTensor,
  tensorBernoulli_DoubleTensor,
  tensorUniform,
  tensorNormal,
  tensorExponential,
  tensorCauchy,
  tensorLogNormal,
  tensorMultinomial,

  // storage functions
  storageSet,
  storageGet,

  storageNew,
  storageNewWithSize,
  storageNewWithSize1,
  storageNewWithSize2,
  storageNewWithSize3,
  storageNewWithSize4,

  storageFree,
  storageResize,
  storageFill,

  // communication requests
  sendTensor,
  sendStorage,

  exit
};

} // namespace thd
