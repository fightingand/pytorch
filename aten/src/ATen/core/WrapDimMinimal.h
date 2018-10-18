#pragma once

#include "c10/util/Exception.h"

namespace at {

static inline int64_t maybe_wrap_dim(int64_t dim, int64_t dim_post_expr, bool wrap_scalar=true) {
  if (dim_post_expr <= 0) {
    AT_CHECK(wrap_scalar, "dimension specified as ", dim, " but tensor has no dimensions");
    dim_post_expr = 1; // this will make range [-1, 0]
  }

  int64_t min = -dim_post_expr;
  int64_t max = dim_post_expr - 1;
  AT_CHECK(
      dim >= min && dim <= max,
      "Dimension out of range (expected to be in range of [",
      min, ", ", max, "], but got ", dim, ")");
  if (dim < 0) dim += dim_post_expr;
  return dim;
}

}
