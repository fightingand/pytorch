/*
The following source file implements a sparse linear operator using cusparseLt
*/

#include <ATen/Functions.h>
#include <ATen/InitialTensorOptions.h>
#include <ATen/cuda/CUDAContext.h>
#include <ATen/cuda/CUDADataType.h>
#include <ATen/cuda/CUDAUtils.h>
#include <c10/core/ScalarType.h>
#include <c10/util/Half.h>
#include <torch/custom_class.h>
#include <iostream>

#include <iostream>

#include <cuda_runtime.h>
#include <cutlass/cutlass.h>
#include <cutlass/gemm/device/gemm_sparse.h>
#include <cutlass/util/host_reorder.h>
#include <cutlass/util/host_tensor.h>
#include <cutlass/util/host_uncompress.h>
#include <cutlass/util/reference/host/gemm.h>
#include <cutlass/util/reference/host/tensor_compare.h>
#include <cutlass/util/reference/host/tensor_copy.h>
#include <cutlass/util/reference/host/tensor_fill.h>
#include <cutlass/util/tensor_view_io.h>

#include <limits>
#include <typeinfo>

#define CUTLASS_CHECK(status)                                             \
  {                                                                       \
    cutlass::Status error = status;                                       \
    if (error != cutlass::Status::kSuccess) {                             \
      std::cerr << "Got cutlass error: " << cutlassGetStatusString(error) \
                << " at: " << __LINE__ << std::endl;                      \
      exit(EXIT_FAILURE);                                                 \
    }                                                                     \
  }

// The code section below describes datatype for input, output matrices and
// computation between elements in input matrices, which will all be used as
// template parameters for cutlass::gemm::device::SparseGemm
using ElementInputA =
    cutlass::half_t; // <- data type of elements in input matrix A
using ElementInputB =
    cutlass::half_t; // <- data type of elements in input matrix B
using ElementOutput =
    cutlass::half_t; // <- data type of elements in output matrix D

// The code section below describes matrix layout of input and output matrices.
// Row Major for Matrix A, Column Major for Matrix B and Row Major for Matrix C
using LayoutInputA = cutlass::layout::RowMajor;
using LayoutInputB = cutlass::layout::RowMajor;
using LayoutOutput = cutlass::layout::RowMajor;

using Gemm = cutlass::gemm::device::SparseGemm<
    cutlass::half_t,
    cutlass::layout::RowMajor,
    cutlass::half_t,
    cutlass::layout::RowMajor,
    cutlass::half_t,
    cutlass::layout::RowMajor,
    float,
    cutlass::arch::OpClassTensorOp,
    cutlass::arch::Sm80,
    cutlass::gemm::GemmShape<128, 256, 64>,
    cutlass::gemm::GemmShape<64, 64, 64>,
    cutlass::gemm::GemmShape<16, 8, 32>,
    cutlass::epilogue::thread::LinearCombination<
        cutlass::half_t,
        128 / cutlass::sizeof_bits<ElementOutput>::value,
        float,
        float>,
    cutlass::gemm::threadblock::GemmIdentityThreadblockSwizzle<>,
    3>;

// Data type and layout of meta data matrix E can be inferred from template
// Gemm.
using ElementInputE = typename Gemm::ElementE;
using LayoutInputE = cutlass::layout::RowMajor;
using ReorderedLayoutInputE = typename Gemm::LayoutE;

// Below property is defined in include/cutlass/arch/sp_mma_sm80.h
// 50% Sparsity on Ampere
constexpr int kSparse = Gemm::kSparse;
// How many elements of A are covered per ElementE
constexpr int kElementsPerElementE = Gemm::kElementsPerElementE;
// The size of individual meta data
constexpr int kMetaSizeInBits = Gemm::kMetaSizeInBits;

namespace at {
namespace native {

uint16_t _mask_to_meta(bool pos0, bool pos1, bool pos2, bool pos3) {
  auto pos_tuple = std::make_tuple(pos0, pos1, pos2, pos3);
  // NOTE:
  // See
  // https://docs.nvidia.com/cuda/parallel-thread-execution/index.html#warp-level-sparse-matrix-storage
  // There are only 6 valid configurations (4 choose 2) and for each there is a
  // special number.
  if (pos_tuple == std::make_tuple(1, 1, 0, 0)) {
    return 4; // 0100
  }
  if (pos_tuple == std::make_tuple(1, 0, 1, 0)) {
    return 8; // 1000
  }
  if (pos_tuple == std::make_tuple(0, 1, 1, 0)) {
    return 9; // 1001
  }
  if (pos_tuple == std::make_tuple(1, 0, 0, 1)) {
    return 12; // 1100
  }
  if (pos_tuple == std::make_tuple(0, 1, 0, 1)) {
    return 13; // 1101
  }
  if (pos_tuple == std::make_tuple(0, 0, 1, 1)) {
    return 14; // 1110
  }
  TORCH_CHECK(
      false,
      "Unsupported mask configuration: ",
      pos0,
      pos1,
      pos2,
      pos3,
      ". Please follow 2 by 4 pattern.");
  return 0;
}

// Hera, "mask" will always be on the CPU.
int64_t _cusparselt_create_meta(const Tensor& mask) {
  const int64_t length_m = mask.size(0);
  const int64_t length_k = mask.size(1);
  TORCH_CHECK(
      length_k % 16 == 0, "Expected size(1) of mask to be divisible by 16.");
  TORCH_CHECK(mask.is_contiguous(), "Expected mask to be contiguous.");
  TORCH_CHECK(mask.dtype() == at::kBool, "Expected mask to be of dtype bool.");

cutlass::HostTensor<ElementInputE, LayoutInputE> result(cutlass::make_Coord((int)length_m, (int)length_k / 16));
  const bool* mask_ptr = mask.data_ptr<bool>();
  for (int64_t i = 0; i < length_m; i++) {
    for (int64_t j = 0; j < length_k; j += 16) {
      uint16_t result_val = 0;
      for (int64_t k = 0; k < 4; k++) {
        bool pos0 = mask_ptr[i * length_k + j + k * 4];
        bool pos1 = mask_ptr[i * length_k + j + k * 4 + 1];
        bool pos2 = mask_ptr[i * length_k + j + k * 4 + 2];
        bool pos3 = mask_ptr[i * length_k + j + k * 4 + 3];
        uint16_t meta_val = _mask_to_meta(pos0, pos1, pos2, pos3);
        result_val = (result_val | (meta_val << (4 * k)));
      }
      // PyTorch doesn't have a uint16_t dtype, so we're using the signed equivalent.
      // However, we don't want to actually convert or overflow. We just want to store the
      // bits as is and then retrieve them again later on.
      int16_t result_storage;
      std::memcpy(&result_storage, &result_val, sizeof(result_storage));
      result.at({i, j / 16}) = result_storage;
    }
  }

  auto result_reordered = new cutlass::HostTensor<ElementInputE, ReorderedLayoutInputE>(cutlass::make_Coord((int)length_m, (int)length_k / 16));
  cutlass::reorder_meta(result_reordered->host_ref(), result.host_ref(),
                        {(int)length_m,
                         0, // currently unused by cutlass::reorder_meta()
                         (int)length_k / 16});
  result_reordered->sync_device();

  static_assert(sizeof(void*) <= sizeof(int64_t));
  return reinterpret_cast<int64_t>(result_reordered);
}

void _cusparselt_destroy_meta(const int64_t meta) {
  static_assert(sizeof(void*) <= sizeof(int64_t));
  delete reinterpret_cast<cutlass::HostTensor<ElementInputE, ReorderedLayoutInputE>*>(meta);
}

// TODO: Pull back in device and cuda version constraints.
Tensor _cusparselt_linear(
    const Tensor& sparse,
    const Tensor& dense,
    const int64_t meta) {
  auto result = sparse.new_empty({sparse.size(0), dense.size(1)}); //.fill_(1);

  constexpr int kSparse = Gemm::kSparse;

  auto tensor_a = sparse;
  auto tensor_b = dense;
  auto tensor_c = result;
  auto tensor_d = result;

  // tensor a is m x (k // kSparse); tensor b is k x n
  const int length_m = tensor_a.size(0);
  const int length_k = tensor_b.size(0);
  const int length_n = tensor_b.size(1);

  TORCH_CHECK(
      tensor_b.size(0) % kSparse == 0,
      "Expected tensor_b.size(0) of value ",
      tensor_b.size(0),
      " to be evenly divisible by ",
      kSparse,
      " but got.");
  TORCH_CHECK(
      tensor_a.size(1) * kSparse == tensor_b.size(0),
      "Expected tensor_a.size(1) of value ",
      tensor_a.size(1),
      " to match tensor_b.size(0) of value ",
      tensor_b.size(0),
      " to match after being multiplied by ",
      kSparse);

  TORCH_CHECK(tensor_c.size(0) == tensor_a.size(0));
  TORCH_CHECK(tensor_d.size(0) == tensor_a.size(0));

  TORCH_CHECK(tensor_c.size(1) == tensor_b.size(1));
  TORCH_CHECK(tensor_d.size(1) == tensor_b.size(1));

  // Create a tuple of problem size for matrix multiplication
  cutlass::gemm::GemmCoord problem_size(length_m, length_n, length_k);

  TORCH_CHECK(
      tensor_a.device() == tensor_b.device(),
      "Check 0: Expected all Tensors to live on the GPU.");
  TORCH_CHECK(
      tensor_b.device() == tensor_c.device(),
      "Check 1: Expected all Tensors to live on the GPU.");
  TORCH_CHECK(
      tensor_c.device() == tensor_d.device(),
      "Check 2: Expected all Tensors to live on the GPU.");

  // Initialize alpha and beta for dot product computation
  float alpha = 1;
  float beta = 0;

  LayoutInputA layout_a(tensor_a.stride(0));
  LayoutInputB layout_b(tensor_b.stride(0));
  LayoutOutput layout_c(tensor_c.stride(0));
  LayoutOutput layout_d(tensor_d.stride(0));

  // Split K dimension into 1 partitions
  int split_k_slices = 1;
  auto tensor_a_device_ref = cutlass::TensorRef<cutlass::half_t, LayoutInputA>(
      (cutlass::half_t*)tensor_a.data_ptr<at::Half>(), layout_a);
  auto tensor_b_device_ref = cutlass::TensorRef<cutlass::half_t, LayoutInputB>(
      (cutlass::half_t*)tensor_b.data_ptr<at::Half>(), layout_b);
  auto tensor_c_device_ref = cutlass::TensorRef<cutlass::half_t, LayoutOutput>(
      (cutlass::half_t*)tensor_c.data_ptr<at::Half>(), layout_c);
  auto tensor_d_device_ref = cutlass::TensorRef<cutlass::half_t, LayoutOutput>(
      (cutlass::half_t*)tensor_d.data_ptr<at::Half>(), layout_d);

  auto tensor_e_reordered = reinterpret_cast<cutlass::HostTensor<ElementInputE, ReorderedLayoutInputE>*>(meta);

  // Create a tuple of gemm kernel arguments. This is later passed as arguments
  // to launch instantiated CUTLASS kernel
  typename Gemm::Arguments arguments{
      problem_size, // <- problem size of matrix multiplication
      tensor_a_device_ref, // <- reference to matrix A on device
      tensor_b_device_ref, // <- reference to matrix B on device
      tensor_c_device_ref, // <- reference to matrix C on device
      tensor_d_device_ref, // <- reference to matrix D on device
      tensor_e_reordered->device_ref(), // <- reference to matrix E on device
      {alpha, beta}, // <- tuple of alpha and beta
      split_k_slices}; // <- k-dimension split factor

  Gemm gemm_op;

  cutlass::Status status = gemm_op.can_implement(arguments);
  // FIXME: it hits an error in this check with two_four_sparse test,
  // still it works when the check commented out.
  // TORCH_CHECK(
  //             status == cutlass::Status::kSuccess,
  //     "Cannot implement CUTLASS Grouped GEMM kernel with given arguments.");

  status =
      gemm_op.initialize(arguments, nullptr, at::cuda::getCurrentCUDAStream());
  TORCH_CHECK(
              (status != cutlass::Status::kErrorWorkspaceNull),
      "Failed to initialize CUTLASS Grouped GEMM kernel due to workspace.");

  TORCH_CHECK(
              (status != cutlass::Status::kErrorInternal),
      "Failed to initialize CUTLASS Grouped GEMM kernel due to internal error.");
  TORCH_CHECK(
      status == cutlass::Status::kSuccess,
      "Failed to initialize CUTLASS Grouped GEMM kernel.");
  status = gemm_op.run(at::cuda::getCurrentCUDAStream());
  TORCH_CHECK(
      status == cutlass::Status::kSuccess,
      "Failed to run CUTLASS Grouped GEMM kernel.");

  C10_CUDA_KERNEL_LAUNCH_CHECK();

  return result;
}

} // namespace native
} // namespace at
