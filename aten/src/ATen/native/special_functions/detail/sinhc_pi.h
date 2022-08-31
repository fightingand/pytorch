#pragma once

#include <ATen/native/special_functions/detail/sinh_pi.h>
#include <c10/util/MathConstants.h>

namespace at {
namespace native {
namespace special_functions {
namespace detail {
template<typename T1>
C10_HOST_DEVICE
promote_t<T1>
sinhc_pi(T1 z) {
  if (std::isnan(z)) {
    return std::numeric_limits<T1>::quiet_NaN();
  } else if (std::abs(c10::pi<T1> * z) < 4 * std::sqrt(std::numeric_limits<T1>::min())) {
    return T1(1) + c10::pi<T1> * z * (c10::pi<T1> * z) / T1(6);
  } else {
    return sinh_pi(z) / (c10::pi<T1> * z);
  }
}
}
}
}
}
