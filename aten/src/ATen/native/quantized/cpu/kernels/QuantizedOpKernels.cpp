#include <ATen/ATen.h>
#include <ATen/Dispatch.h>
#include <ATen/native/TensorIterator.h>
#include <ATen/native/cpu/Loops.h>
#include <ATen/native/quantized/cpu/quantized_ops.h>

namespace at {
namespace native {
namespace {

// ****************** HEY YOU! YES YOU! Read this! ********************
//
// Please read the README.md in this directory before editing this file

void qrelu_kernel(const Tensor& qx, Tensor& qy) {
  const auto zero_point = qx.q_zero_point();
  AT_DISPATCH_QINT_TYPES(qx.scalar_type(), "qrelu", [&]() {
    qy = at::_empty_affine_quantized(
        qx.sizes(),
        at::device(kCPU).dtype(SCALAR_TYPE),
        qx.q_scale(),
        qx.q_zero_point(),
        qx.suggest_memory_format());
    using Vec = Vec256<scalar_t>;
    auto zero_point_vec = Vec(scalar_t(zero_point));
    auto iter = TensorIterator::unary_op(qy, qx);
    cpu_kernel_vec(
        iter,
        [&](scalar_t value) -> scalar_t {
          return scalar_t(std::max<underlying_t>(value.val_, zero_point));
        },
        [&](Vec value) -> Vec { return value.relu(zero_point_vec); });
  });
}

void qrelu6_kernel(const Tensor& qx, Tensor& qy) {
  const auto zero_point = qx.q_zero_point();
  AT_DISPATCH_QINT_TYPES(qx.scalar_type(), "qrelu6", [&]() {
    qy = at::_empty_affine_quantized(
        qx.sizes(),
        at::device(kCPU).dtype(SCALAR_TYPE),
        qx.q_scale(),
        qx.q_zero_point(),
        qx.suggest_memory_format());
    using Vec = Vec256<scalar_t>;
    auto iter = TensorIterator::unary_op(qy, qx);
    scalar_t six =
        at::quantize_val<scalar_t>(qx.q_scale(), qx.q_zero_point(), 6.0);
    auto zero_point_vec = Vec(scalar_t(zero_point));
    auto six_vec = Vec(six);
    cpu_kernel_vec(
        iter,
        [&](scalar_t value) -> scalar_t {
          underlying_t relu_val =
              std::max<underlying_t>(value.val_, zero_point);
          return scalar_t(std::min<underlying_t>(relu_val, six.val_));
        },
        [&](Vec val) -> Vec { return val.relu6(zero_point_vec, six_vec); });
  });
}

// Note: out is assumed to be the same size as self and other.
// Note: Addition is only supported when self, other, out are of the same dtype.
template <bool ReLUFused = false>
void qadd_kernel(Tensor& out, const Tensor& self, const Tensor& other) {
  int64_t zero_point = out.q_zero_point();
  float scale = out.q_scale();
  float inv_scale = 1.0f / scale;
  int64_t self_zero_point = self.q_zero_point();
  float self_scale = self.q_scale();
  int64_t other_zero_point = other.q_zero_point();
  float other_scale = other.q_scale();

  // Broadcast out the parameters here to amortize out that cost across
  // loop iterations.
  // TODO: we can optimize dequantization by doing a premultiplication
  // of the zero point by scale and doing FMA on scale*x_q - (scale*zero_point)
  auto self_zero_point_vec = Vec256<float>((float)self_zero_point);
  auto self_scale_vec = Vec256<float>(self_scale);
  auto other_zero_point_vec = Vec256<float>((float)other_zero_point);
  auto other_scale_vec = Vec256<float>(other_scale);

  auto self_scale_zp_premul_vec = self_scale_vec * self_zero_point_vec.neg();
  auto other_scale_zp_premul_vec = other_scale_vec * other_zero_point_vec.neg();

  auto iter = TensorIterator::binary_op(out, self, other);

  AT_DISPATCH_QINT_TYPES(out.scalar_type(), "qadd", [&]() {
    using Vec = Vec256<scalar_t>;
    cpu_kernel_vec(
        iter,
        [&](scalar_t a, scalar_t b) -> scalar_t {
          const auto da = at::dequantize_val(self_scale, self_zero_point, a);
          const auto db = at::dequantize_val(other_scale, other_zero_point, b);
          float c = da + db;
          if (ReLUFused) {
            c = std::max<float>(c, 0.0);
          }
          return at::quantize_val<scalar_t>(scale, zero_point, c);
        },
        [&](Vec a, Vec b) -> Vec {
          const auto da = a.dequantize(
              self_scale_vec, self_zero_point_vec, self_scale_zp_premul_vec);
          const auto db = b.dequantize(
              other_scale_vec, other_zero_point_vec, other_scale_zp_premul_vec);
          Vec::float_vec_return_type retvals;
          for (int i = 0; i < Vec::float_num_vecs(); ++i) {
            auto c = da[i] + db[i];
            if (ReLUFused) {
              c = vec256::maximum(c, Vec256<float>(0.0f));
            }
            retvals[i] = c;
          }
          // TODO: fbgemm::Quantize doesn't support taking in the
          // pre-broadcasted parameters. We might be able to save some cycles by
          // enabling that in the API.
          // TODO: specialize fbgemm::Quantize for a single vector and make it
          // inlineable. This could help with interleaving as suggested by the
          // TensorIterator implementations
          auto rv = Vec::quantize(retvals, scale, zero_point, inv_scale);
          return rv;
        });
  });
}

void qmaxpool_2d_nhwc_kernel(
    const Tensor& qx,
    int64_t iC, // input/output channels
    int64_t iH,
    int64_t iW, // input sizes
    int64_t oH,
    int64_t oW, // output sizes
    int64_t kH,
    int64_t kW, // kernel size
    int64_t sH,
    int64_t sW, // strides
    int64_t pH,
    int64_t pW, // padding
    int64_t dH,
    int64_t dW, // dilation
    Tensor& qy) {
  AT_DISPATCH_QINT_TYPES(qx.scalar_type(), "max_pool2d_nhwc", [&]() {
    scalar_t* idata = static_cast<scalar_t*>(qx.data_ptr());
    scalar_t* odata = static_cast<scalar_t*>(qy.data_ptr());

    // Loop over N
    for (int64_t b = 0; b < qx.size(0); ++b) {
      // Loop over H
      auto* i_p =
          reinterpret_cast<scalar_t::underlying*>(idata + b * iW * iH * iC);
      for (int64_t row = 0; row < oH; ++row) {
        // Loop over W
        for (int64_t col = 0; col < oW; ++col) {
          // Pointer to output data for this specific N,H,W position
          auto* o_p = reinterpret_cast<scalar_t::underlying*>(
              odata + b * oH * oW * iC + row * oW * iC + col * iC);

          // Loop over reduction block
          int64_t h_start = row * sH - pH;
          int64_t w_start = col * sW - pW;
          int64_t h_end = std::min(h_start + (kH - 1) * dH + 1, iH);
          int64_t w_end = std::min(w_start + (kW - 1) * dW + 1, iW);
          while (h_start < 0)
            h_start += dH;
          while (w_start < 0)
            w_start += dW;

          int64_t c = 0;

          // Interleaved vector loop 4x
          constexpr auto vec_width = Vec256<scalar_t>::size();
          for (; c + 4 * vec_width <= iC; c += 4 * vec_width) {
            Vec256<scalar_t> acc{
                scalar_t(std::numeric_limits<scalar_t::underlying>::lowest())};
            Vec256<scalar_t> accs[4] = {acc, acc, acc, acc};
            int64_t tcntr = 0;
            int64_t x, y;
            for (y = h_start; y < h_end; y += dH) {
              for (x = w_start; x < w_end; x += dW) {
                for (int i = 0; i < 4; ++i) {
                  tcntr = y * iW + x;
                  auto vals = Vec256<scalar_t>::loadu(
                      i_p + tcntr * iC + c + Vec256<scalar_t>::size() * i);
                  accs[i] = vec256::maximum(accs[i], vals);
                }
              } // for x
            } // for y
            for (int i = 0; i < 4; ++i) {
              accs[i].store(o_p + c + Vec256<scalar_t>::size() * i);
            }
          } // for c

          // Vector loop
          for (; c + vec_width <= iC; c += vec_width) {
            Vec256<scalar_t> acc{
                scalar_t(std::numeric_limits<scalar_t::underlying>::lowest())};
            int64_t tcntr = 0;
            int64_t x, y;
            for (y = h_start; y < h_end; y += dH) {
              for (x = w_start; x < w_end; x += dW) {
                tcntr = y * iW + x;
                auto vals = Vec256<scalar_t>::loadu(i_p + tcntr * iC + c);
                acc = vec256::maximum(acc, vals);
              } // for x
            } // for y
            acc.store(o_p + c);
          } // for c

          for (; c < iC; ++c) {
            auto max_val = std::numeric_limits<scalar_t::underlying>::lowest();
            int64_t tcntr = 0;
            int64_t x, y;
            for (y = h_start; y < h_end; y += dH) {
              for (x = w_start; x < w_end; x += dW) {
                tcntr = y * iW + x;
                auto val = *(i_p + tcntr * iC + c);
                max_val = std::max(max_val, val);
              } // for x
            } // for y

            o_p[c] = max_val;
          } // for c
        } // for col
      } // for row
    } // for b
  });
}

void qadaptive_avg_pool2d_nhwc_kernel(
    const Tensor& qx,
    Tensor& qy,
    int64_t b,
    int64_t sizeD,
    int64_t isizeH,
    int64_t isizeW,
    int64_t osizeH,
    int64_t osizeW,
    int64_t istrideB,
    int64_t istrideD,
    int64_t istrideH,
    int64_t istrideW) {
  AT_DISPATCH_QINT_TYPES(qx.scalar_type(), "adaptive_avg_pool2d_nhwc", [&]() {
    scalar_t* idata = static_cast<scalar_t*>(qx.data_ptr());
    scalar_t* odata = static_cast<scalar_t*>(qy.data_ptr());
    auto minimum = std::numeric_limits<scalar_t::underlying>::lowest();
    auto maximum = std::numeric_limits<scalar_t::underlying>::max();
    auto* i_p =
        reinterpret_cast<typename scalar_t::underlying*>(idata + b * istrideB);
    for (int64_t oh = 0; oh < osizeH; oh++) {
      int istartH = (int)std::floor((float)(oh * isizeH) / osizeH);
      int iendH = (int)std::ceil((float)((oh + 1) * isizeH) / osizeH);
      int kH = iendH - istartH;
      for (int64_t ow = 0; ow < osizeW; ow++) {
        auto* o_p = reinterpret_cast<typename scalar_t::underlying*>(
            odata + b * osizeH * osizeW * sizeD + (oh * osizeW + ow) * sizeD);
        int istartW = (int)std::floor((float)(ow * isizeW) / osizeW);
        int iendW = (int)std::ceil((float)((ow + 1) * isizeW) / osizeW);
        int kW = iendW - istartW;
        int size = kH * kW;
        float multiplier = qx.q_scale() / qy.q_scale() / size;
        int64_t c = 0;
        // For int8 or uint8quantization, we implicitly use int32 as
        // accumulation Or else, it will go to the slow path
        // TODO: support 16bit, 32bit, and etc.
        constexpr auto vec_width = Vec256<scalar_t>::size() / 4;
        auto* internal_i_p = i_p + istartH * istrideH + istartW * istrideW;

        // TODO: more vectorization with loop interleaving
#ifdef __AVX2__
        if (vec_width == 8) {
          for (; c + vec_width <= sizeD; c += vec_width) {
            int64_t tcntr = 0;
            Vec256<int32_t> acc(-qx.q_zero_point() * size);
            for (int64_t ih = 0; ih < kH; ih++) {
              for (int64_t iw = 0; iw < kW; iw++) {
                tcntr = ih * istrideH + iw * istrideW;
                auto vals =
                    vec256::convert_to_int32<typename scalar_t::underlying>(
                        internal_i_p + tcntr + c * istrideD);
                acc = acc + vals;
              }
            }
            int32_t acc_int[vec_width];
            float acc_fp[vec_width];
            acc.store(acc_int);
            vec256::convert(acc_int, acc_fp, vec_width);
            vec256::QuantizeAvx2<scalar_t>(
                acc_fp, o_p + c, vec_width, multiplier, qy.q_zero_point());
          }
        }
#endif
        // remainer
        for (; c < sizeD; ++c) {
          int32_t acc_int32 = -qx.q_zero_point() * size;
          int64_t tcntr = 0;
          for (int64_t ih = 0; ih < kH; ih++) {
            for (int64_t iw = 0; iw < kW; iw++) {
              tcntr = ih * istrideH + iw * istrideW;
              auto val = *(internal_i_p + tcntr + c * istrideD);
              acc_int32 += val;
            }
          }
          // clamp
          o_p[c] = std::min<int32_t>(
              std::max<int32_t>(
                  std::nearbyint(acc_int32 * multiplier + qy.q_zero_point()),
                  minimum),
              maximum);
        } // c
      } // oh
    } // ow
  });
}

void qavg_pool2d_nhwc_kernel(
    const Tensor& qx,
    Tensor& qy,
    int64_t b,
    int64_t nInputPlane,
    int64_t inputWidth,
    int64_t inputHeight,
    int64_t outputWidth,
    int64_t outputHeight,
    int kW,
    int kH,
    int dW,
    int dH,
    int padW,
    int padH,
    bool count_include_pad,
    c10::optional<int64_t> divisor_override) {
  AT_DISPATCH_QINT_TYPES(qx.scalar_type(), "avg_pool2d_nhwc", [&]() {
    scalar_t* idata = static_cast<scalar_t*>(qx.data_ptr());
    scalar_t* odata = static_cast<scalar_t*>(qy.data_ptr());
    auto minimum = std::numeric_limits<scalar_t::underlying>::lowest();
    auto maximum = std::numeric_limits<scalar_t::underlying>::max();
    int64_t batch_size = nInputPlane * inputWidth * inputHeight;
    auto* i_p = reinterpret_cast<typename scalar_t::underlying*>(
        idata + b * batch_size);

    for (int64_t oh = 0; oh < outputHeight; oh++) {
      for (int64_t ow = 0; ow < outputWidth; ow++) {
        auto* o_p = reinterpret_cast<typename scalar_t::underlying*>(
            odata + b * nInputPlane * outputWidth * outputHeight +
            (oh * outputWidth + ow) * nInputPlane);
        int64_t hstart = oh * dH - padH;
        int64_t wstart = ow * dW - padW;
        int64_t hend = std::min(hstart + kH, inputHeight + padH);
        int64_t wend = std::min(wstart + kW, inputWidth + padW);
        int64_t pool_size = (hend - hstart) * (wend - wstart);
        hstart = std::max(hstart, (int64_t)0);
        wstart = std::max(wstart, (int64_t)0);
        hend = std::min(hend, inputHeight);
        wend = std::min(wend, inputWidth);

        int64_t size;
        int64_t divide_factor;
        if (divisor_override.has_value()) {
          divide_factor = divisor_override.value();
          size = (hend - hstart) * (wend - wstart);
        } else {
          if (count_include_pad) {
            divide_factor = pool_size;
          } else {
            divide_factor = (hend - hstart) * (wend - wstart);
          }
          size = divide_factor;
        }

        int64_t c = 0;
        // For int8 quantization, we implicitly use int32 as accumulation
        // Or else, it will go to the slow path
        // TODO: support 16bit, 32bit, and etc.
        constexpr auto vec_width = Vec256<scalar_t>::size() / 4;
        float multiplier = qx.q_scale() / qy.q_scale() / divide_factor;
#ifdef __AVX2__
        if (vec_width == 8) {
          for (; c + vec_width <= nInputPlane; c += vec_width) {
            int64_t tcntr = 0;
            Vec256<int32_t> acc(-qx.q_zero_point() * size);
            for (int64_t ih = hstart; ih < hend; ih++) {
              for (int64_t iw = wstart; iw < wend; iw++) {
                tcntr = ih * inputWidth + iw;
                auto vals =
                    vec256::convert_to_int32<typename scalar_t::underlying>(
                        i_p + tcntr * nInputPlane + c);
                acc = acc + vals;
              }
            }
            int32_t acc_int[vec_width];
            float acc_fp[vec_width];
            acc.store(acc_int);
            vec256::convert(acc_int, acc_fp, vec_width);
            vec256::QuantizeAvx2<scalar_t>(
                acc_fp, o_p + c, vec_width, multiplier, qy.q_zero_point());
          }
        }
#endif
        // remainer
        for (; c < nInputPlane; ++c) {
          int32_t acc_int32 = -qx.q_zero_point() * size;
          int64_t tcntr = 0;
          for (int64_t ih = hstart; ih < hend; ih++) {
            for (int64_t iw = wstart; iw < wend; iw++) {
              tcntr = ih * inputWidth + iw;
              auto val = *(i_p + tcntr * nInputPlane + c);
              acc_int32 += val;
            }
          }
          double acc_fp = acc_int32 * 1.0;
          // clamp
          o_p[c] = std::min<int32_t>(
              std::max<int32_t>(
                  std::nearbyint(acc_fp * multiplier + qy.q_zero_point()),
                  minimum),
              maximum);
        } // c
      } // ow
    } // oh
  });
}

} // namespace

REGISTER_DISPATCH(qrelu_stub, &qrelu_kernel);
REGISTER_DISPATCH(qrelu6_stub, &qrelu6_kernel);
REGISTER_DISPATCH(qadd_relu_stub, &qadd_kernel<true>);
REGISTER_DISPATCH(qadd_stub, &qadd_kernel<false>);
REGISTER_DISPATCH(qmaxpool_2d_nhwc_stub, &qmaxpool_2d_nhwc_kernel);
REGISTER_DISPATCH(
    qadaptive_avg_pool2d_nhwc_stub,
    &qadaptive_avg_pool2d_nhwc_kernel);
REGISTER_DISPATCH(qavg_pool2d_nhwc_stub, &qavg_pool2d_nhwc_kernel);

} // namespace native
} // namespace at
