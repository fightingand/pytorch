#pragma once

#include "ATen/Tensor.h"
#include "ATen/TensorImpl.h"
#include "ATen/Error.h"

namespace at {
struct SparseTensorImpl : public TensorImpl {
  // Stored in COO format, indices + values

  std::vector<int64_t> size_;
  // INVARIANT: indices_.size(1) >= nnz_ && values_.size(0) >= nnz_
  int64_t nnz_ = 0;
  int64_t dimI_ = 0; // number of sparse dimensions
  int64_t dimV_ = 0; // number of dense dimensions

  // 2-D tensor of nDim x nnz of indices. May have nnz dim bigger than nnz
  // as buffer, so we keep track of both
  Tensor indices_; // always a LongTensor
  Tensor values_;

  // A sparse tensor is 'coalesced' if every index occurs at most once in
  // the indices tensor, and the indices are in sorted order.
  // Most math operations can only be performed on ordered sparse tensors
  bool coalesced_ = false;

public:
  // Public for now...
  explicit SparseTensorImpl(Type * type);

  int64_t nnz() const { return nnz_; }
  int64_t dimI() const { return dimI_; }
  int64_t dimV() const { return dimV_; }
  bool coalesced() const { return coalesced_; }
  Tensor indices() const { return indices_; }
  Tensor values() const { return values_; }

  const char * toString() const override;
  IntList sizes() const override;
  IntList strides() const override;
  int64_t dim() const override;
  Scalar localScalar() override;
  void * unsafeGetTH(bool retain) override;
  std::unique_ptr<Storage> storage() override;

  // Some ops do some manual size fiddling.
  // TODO: Figure out a more safe way to provide this functionality
  std::vector<int64_t>& _sizes_mut() { return size_; }

  // WARNING: This function does NOT preserve invariants of dimI/dimV with
  // respect to indices and values
  void raw_resize_(int64_t dimI, int64_t dimV, ArrayRef<int64_t> size) {
    // TODO: is this the correct idiom?
    AT_CHECK(size.size() != 0, "cannot resize a sparse tensor into a scalar");
    size_ = size;
    dimI_ = dimI;
    dimV_ = dimV;
  }

  // TODO: I hate these two setters, please get rid of them!!!
  void set_indices(const Tensor& indices) {
    AT_ASSERT(indices.type().backend() == type().backend());
    AT_ASSERT(indices.type().scalarType() == kLong);
    indices_ = indices;
  }
  void set_values(const Tensor& values) {
    AT_ASSERT(values.type().toSparse() == type());
    values_ = values;
  }

  void set_coalesced(bool coalesced) { coalesced_ = coalesced; }
  void set_nnz(int64_t nnz) { nnz_ = nnz; }

  // This used to be called THSTensor_(_move)
  // NB: This used to be able to avoid a refcount bump, but I was too lazy to
  // make it happen
  void set_indices_and_values(const Tensor& indices, const Tensor& values);
};

} // namespace at
