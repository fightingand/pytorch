#ifndef CAFFE2_FILLER_H_
#define CAFFE2_FILLER_H_

#include <sstream>

#include "caffe2/core/logging.h"
#include "caffe2/core/tensor.h"

namespace caffe2 {

// TODO: replace filler distribution enum with a better abstraction
enum FillerDistribution { FD_UNIFORM, FD_FIXEDSUM, FD_SYNTHETIC };

class TensorFiller {
 public:
  // Note: to avoid including an MKL dependency, we are having the
  // implementation of the Fill functiomn in the filler_impl.h file.
  template <class Type, class Context>
  void Fill(Tensor* tensor, Context* context) const;

  TensorFiller& Dist(FillerDistribution dist) {
    dist_ = dist;
    return *this;
  }

  template <class Type>
  TensorFiller& Min(Type min) {
    min_ = (double)min;
    return *this;
  }

  template <class Type>
  TensorFiller& Max(Type max) {
    max_ = (double)max;
    return *this;
  }

  template <class Type>
  TensorFiller& FixedSum(Type fixed_sum) {
    dist_ = FD_FIXEDSUM;
    fixed_sum_ = (double)fixed_sum;
    return *this;
  }

  // a helper function to construct the lengths vector for sparse features
  template <class Type>
  TensorFiller& SparseLengths(Type total_length) {
    return FixedSum(total_length).Min(0).Max(total_length);
  }

  // a helper function to construct the segments vector for sparse features
  template <class Type>
  TensorFiller& SparseSegments(Type max_segment) {
    CAFFE_ENFORCE(dist_ != FD_FIXEDSUM);
    return Min(0).Max(max_segment).Dist(FD_SYNTHETIC);
  }

  TensorFiller& Shape(const std::vector<int64_t>& shape) {
    shape_ = shape;
    return *this;
  }

  template <class Type>
  TensorFiller(const std::vector<int64_t>& shape, Type fixed_sum)
      : shape_(shape), dist_(FD_FIXEDSUM), fixed_sum_((double)fixed_sum) {}

  TensorFiller(const std::vector<int64_t>& shape)
      : shape_(shape), dist_(FD_UNIFORM), fixed_sum_(0) {}

  TensorFiller() : TensorFiller(std::vector<int64_t>()) {}

  std::string DebugString() const {
    std::stringstream stream;
    stream << "shape = [" << shape_ << "]; min = " << min_
           << "; max = " << max_;
    switch (dist_) {
      case FD_FIXEDSUM:
        stream << "; dist = FD_FIXEDSUM";
        break;
      case FD_SYNTHETIC:
        stream << "; dist = FD_SYNTHETIC";
        break;
      default:
        stream << "; dist = FD_UNIFORM";
        break;
    }
    return stream.str();
  }

 private:
  std::vector<int64_t> shape_;
  // TODO: type is unknown until a user starts to fill data;
  // cast everything to double for now.
  double min_ = 0.0;
  double max_ = 1.0;
  FillerDistribution dist_;
  double fixed_sum_;
};

} // namespace caffe2

#endif // CAFFE2_FILLER_H_
