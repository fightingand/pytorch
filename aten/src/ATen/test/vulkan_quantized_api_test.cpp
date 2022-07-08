#ifdef USE_VULKAN_API

#include <ATen/ATen.h>
#include <ATen/core/dispatch/Dispatcher.h>
#include <ATen/native/vulkan/api/api.h>
#include <gtest/gtest.h>

#include <ATen/native/vulkan/ops/Common.h>
#include <ATen/native/vulkan/ops/Copy.h>
#include <ATen/native/vulkan/ops/Factory.h>
#include <ATen/native/vulkan/ops/QuantizedFunctions.h>

#include <c10/util/irange.h>

namespace {

bool checkRtol(const at::Tensor& diff, const std::vector<at::Tensor>& inputs) {
  float maxValue = 0.0f;

  for (const auto& tensor : inputs) {
    maxValue = fmax(tensor.abs().max().item<float>(), maxValue);
  }

#ifdef USE_VULKAN_FP16_INFERENCE
  constexpr float tolerance = 1e-2;
#else
  constexpr float tolerance = 1e-5;
#endif

  return diff.abs().max().item<float>() <= (tolerance * maxValue);
}

bool almostEqual(const at::Tensor& a, const at::Tensor& b) {
  return checkRtol(a - b, {a, b});
}

bool exactlyEqual(const at::Tensor& a, const at::Tensor& b) {
  return (a - b).abs().max().item<float>() == 0.0f;
}

void showRtol(const at::Tensor& a, const at::Tensor& b) {
  const auto diff = (a - b).abs();

  float maxValue = a.abs().max().item<float>();
  maxValue = fmax(b.abs().max().item<float>(), maxValue);

#ifdef USE_VULKAN_FP16_INFERENCE
  constexpr float tolerance = 1e-2;
#else
  constexpr float tolerance = 1e-5;
#endif

  const float maxDiff = maxValue * tolerance;
  std::cout << "Max Diff allowed: " << maxDiff << std::endl;
  if (diff.sizes().size() == 2) {
    for (const auto y : c10::irange(diff.sizes()[0])) {
      std::cout << y << ":";
      for (const auto x : c10::irange(diff.sizes()[1])) {
        float diff_xy = diff[y][x].item<float>();
        if (diff_xy > maxDiff) {
          std::cout << std::setw(5) << x;
        } else {
          std::cout << std::setw(5) << " ";
        }
      }
      std::cout << std::endl;
    }
  }
}
} // namespace

namespace {

class VulkanAPITest : public ::testing::Test {
 public:
#if defined(__ANDROID__) // to avoid `Undefined symbols for architecture arm64`
                         // error
  static void SetUpTestSuite() {
    at::native::vulkan::api::context()->querypool().enable();
  }

  static void TearDownTestSuite() {
    at::native::vulkan::api::context()->querypool().disable(false);
  }
#endif
};

at::Tensor cpu_to_vulkan(at::Tensor in_cpu) {
  auto options = in_cpu.options();
  if (options.dtype().toScalarType() == c10::ScalarType::QUInt8) {
    auto ret = at::native::vulkan::ops::_empty_affine_quantized(
      in_cpu.sizes(),
      c10::ScalarType::QUInt8,
      options.layout(),
      options.device(),
      options.pinned_memory(),
      in_cpu.q_scale(),
      in_cpu.q_zero_point(),
      c10::MemoryFormat::Contiguous);
    at::native::vulkan::ops::copy_(ret, in_cpu);
    return ret;
  } else {
    auto ret = at::empty(in_cpu.sizes(), options);
    at::native::vulkan::ops::copy_(ret, in_cpu);
    return ret;
  }
}

at::Tensor vulkan_to_cpu(at::Tensor vulkan, at::Tensor in_cpu) {
  auto q_options = in_cpu.options();
  if (q_options.dtype().toScalarType() == c10::ScalarType::QUInt8) {
    auto output = at::native::empty_affine_quantized(
        in_cpu.sizes(),
        q_options.dtype().toScalarType(),
        q_options.layout(),
        q_options.device(),
        q_options.pinned_memory(),
        in_cpu.q_scale(),
        in_cpu.q_zero_point());
    at::native::vulkan::ops::copy_(output, vulkan);
    return output;
  } else {
    auto output = at::empty(in_cpu.sizes(), q_options);
    at::native::vulkan::ops::copy_(output, vulkan);
    return output;
  }
}

TEST_F(VulkanAPITest, support_vulkan) {
  if (!at::is_vulkan_available()) {
    return;
  }

  const double scale = 0.1;
  const int64_t zero_point = 10;

  auto in_cpu =
      at::rand({2, 13, 32, 27}, at::device(at::kCPU).dtype(at::kFloat)) * 12 - 6;
  auto in_cpu_quantized = at::quantize_per_tensor(
      in_cpu, scale, zero_point, c10::ScalarType::QUInt8);

  auto in_vulkan_quantized = cpu_to_vulkan(in_cpu_quantized);
  at::native::vulkan::api::PipelineBarrier pipeline_barrier{};
  at::native::vulkan::ops::vTensor& v_self =
      at::native::vulkan::ops::convert(in_vulkan_quantized);
  if (in_cpu.dtype() == c10::kQUInt8) {
    v_self.image(
        pipeline_barrier,
        at::native::vulkan::api::PipelineStage::COMPUTE,
        at::native::vulkan::api::MemoryAccessType::READ);
    v_self.image(
        pipeline_barrier,
        at::native::vulkan::api::PipelineStage::COMPUTE,
        at::native::vulkan::api::MemoryAccessType::WRITE);
  }
  auto output = vulkan_to_cpu(in_vulkan_quantized, in_cpu_quantized);
  const auto check = almostEqual(
      at::native::int_repr_quantized_cpu(in_cpu_quantized),
      at::native::int_repr_quantized_cpu(output));

  if (!check) {
    showRtol(
        at::native::int_repr_quantized_cpu(in_cpu_quantized),
        at::native::int_repr_quantized_cpu(output));
  }

  ASSERT_TRUE(check);
}

TEST_F(VulkanAPITest, quantize_per_tensor) {
  if (!at::is_vulkan_available()) {
    return;
  }

  const auto in_cpu =
      at::rand({2, 13, 32, 27}, at::device(at::kCPU).dtype(at::kFloat)) * 6;
  const auto in_vulkan = in_cpu.vulkan();

  const double scale = 0.1;
  const int zero_point = 10;

  const auto out_cpu = at::quantize_per_tensor(
      in_cpu, scale, zero_point, c10::ScalarType::QUInt8);
  const auto out_vulkan = at::native::vulkan::ops::quantize_per_tensor(
      in_vulkan, scale, zero_point, c10::ScalarType::QUInt8);

  auto output_for_quantized_vulkan =
      vulkan_to_cpu(out_vulkan, out_cpu);

  int rtol = 1;
  const auto check = at::allclose(
      at::native::int_repr_quantized_cpu(out_cpu),
      at::native::int_repr_quantized_cpu(output_for_quantized_vulkan),
      rtol);

  if (!check) {
    std::cout << "Max Diff allowed: " << rtol << std::endl;
  }

  ASSERT_TRUE(check);
}

TEST_F(VulkanAPITest, quantize_dequantize) {
  if (!at::is_vulkan_available()) {
    return;
  }

  const auto in_cpu =
      at::rand({2, 13, 32, 27}, at::device(at::kCPU).dtype(at::kFloat)) * 6;
  const auto in_vulkan = in_cpu.vulkan();

  const double scale = 0.1;
  const int zero_point = 10;
  // quantize tensors
  const auto out_cpu = at::quantize_per_tensor(
      in_cpu, scale, zero_point, c10::ScalarType::QUInt8);
  const auto out_vulkan = at::native::vulkan::ops::quantize_per_tensor(
      in_vulkan, scale, zero_point, c10::ScalarType::QUInt8);
  // dequantize tensors
  const auto out_cpu_deq = at::dequantize(out_cpu);
  const auto out_vulkan_deq = at::native::vulkan::ops::dequantize(out_vulkan);
  auto output_for_dequantized_vulkan =
      vulkan_to_cpu(out_vulkan_deq, in_cpu);

  float rtol = 1;
  float atol = 0.5;
  const auto check = at::allclose(in_cpu, output_for_dequantized_vulkan, rtol, atol);

  if (!check) {
    std::cout << "Max Diff allowed: " << rtol << std::endl;
  }

  ASSERT_TRUE(check);

  const auto check_two =
      at::allclose(out_cpu_deq, output_for_dequantized_vulkan, rtol, atol);

  if (!check_two) {
    std::cout << "Max Diff allowed: " << rtol << std::endl;
  }

  ASSERT_TRUE(check_two);
}

TEST_F(VulkanAPITest, quantized_add) {
  if (!at::is_vulkan_available()) {
    return;
  }

  const auto in_cpu =
      at::rand({2, 13, 32, 27}, at::device(at::kCPU).dtype(at::kFloat)) * 6;
  const auto in_vulkan = in_cpu.vulkan();
  const auto in_cpu2 =
      at::rand({2, 13, 32, 27}, at::device(at::kCPU).dtype(at::kFloat)) * 6;
  const auto in_vulkan2 = in_cpu2.vulkan();

  const double scale = 0.1;
  const int zero_point = 10;

  const auto out_cpu = at::quantize_per_tensor(
      in_cpu, scale, zero_point, c10::ScalarType::QUInt8);
  const auto out_cpu2 = at::quantize_per_tensor(
      in_cpu2, scale, zero_point, c10::ScalarType::QUInt8);
  const auto out_vulkan = at::native::vulkan::ops::quantize_per_tensor(
      in_vulkan, scale, zero_point, c10::ScalarType::QUInt8);
  const auto out_vulkan2 = at::native::vulkan::ops::quantize_per_tensor(
      in_vulkan2, scale, zero_point, c10::ScalarType::QUInt8);

  auto out_vulkan_to_cpu = at::native::int_repr_quantized_cpu(vulkan_to_cpu(out_vulkan, out_cpu));
  auto out_vulkan_to_cpu2 = at::native::int_repr_quantized_cpu(vulkan_to_cpu(out_vulkan2, out_cpu2));

  const auto reg_added_tensors = at::add(in_cpu, in_cpu2);

  const double scale3 = 0.15;
  const int zero_point3 = 15;
  const auto vulk_added_tensors = at::native::vulkan::ops::quantized_add(out_vulkan, out_vulkan2, scale3, zero_point3);

  const auto out_vulkan_deq = at::native::vulkan::ops::dequantize(vulk_added_tensors);
  auto output_for_dequantized_vulkan = vulkan_to_cpu(out_vulkan_deq, in_cpu2);

  float rtol = 0;
  float atol = 0.5;
  const auto check = at::allclose(reg_added_tensors, output_for_dequantized_vulkan, rtol, atol);

  if (!check) {
    std::cout << "Max Diff allowed: " << rtol << std::endl;
  }

  ASSERT_TRUE(check);
}

TEST_F(VulkanAPITest, quantized_add_broadcast) {
  if (!at::is_vulkan_available()) {
    return;
  }

  const auto in_cpu =
      at::rand({2, 13, 1, 1}, at::device(at::kCPU).dtype(at::kFloat)) * 6;
  const auto in_vulkan = in_cpu.vulkan();
  const auto in_cpu2 =
      at::rand({2, 13, 32, 27}, at::device(at::kCPU).dtype(at::kFloat)) * 6;
  const auto in_vulkan2 = in_cpu2.vulkan();

  const double scale = 0.1;
  const int zero_point = 10;

  const auto out_cpu = at::quantize_per_tensor(
      in_cpu, scale, zero_point, c10::ScalarType::QUInt8);
  const auto out_cpu2 = at::quantize_per_tensor(
      in_cpu2, scale, zero_point, c10::ScalarType::QUInt8);
  const auto out_vulkan = at::native::vulkan::ops::quantize_per_tensor(
      in_vulkan, scale, zero_point, c10::ScalarType::QUInt8);
  const auto out_vulkan2 = at::native::vulkan::ops::quantize_per_tensor(
      in_vulkan2, scale, zero_point, c10::ScalarType::QUInt8);

  auto out_vulkan_to_cpu = at::native::int_repr_quantized_cpu(vulkan_to_cpu(out_vulkan, out_cpu));
  auto out_vulkan_to_cpu2 = at::native::int_repr_quantized_cpu(vulkan_to_cpu(out_vulkan2, out_cpu2));

  const auto reg_added_tensors = at::add(in_cpu, in_cpu2);

  const double scale3 = 0.15;
  const int zero_point3 = 15;
  const auto vulk_added_tensors = at::native::vulkan::ops::quantized_add(out_vulkan, out_vulkan2, scale3, zero_point3);

  const auto out_vulkan_deq = at::native::vulkan::ops::dequantize(vulk_added_tensors);
  auto output_for_dequantized_vulkan = vulkan_to_cpu(out_vulkan_deq, in_cpu2);

  float rtol = 0;
  float atol = 0.5;
  const auto check = at::allclose(reg_added_tensors, output_for_dequantized_vulkan, rtol, atol);

  if (!check) {
    std::cout << "Max Diff allowed: " << rtol << std::endl;
  }

  ASSERT_TRUE(check);
}

TEST_F(VulkanAPITest, quantized_add_dif_params) {
  if (!at::is_vulkan_available()) {
    return;
  }

  const auto in_cpu =
      at::rand({2, 13, 32, 27}, at::device(at::kCPU).dtype(at::kFloat)) * 6;
  const auto in_vulkan = in_cpu.vulkan();
  const auto in_cpu2 =
      at::rand({2, 13, 32, 27}, at::device(at::kCPU).dtype(at::kFloat)) * 6;
  const auto in_vulkan2 = in_cpu2.vulkan();
  const double scale = 0.1;
  const int zero_point = 10;
  const double scale2 = 0.2;
  const int zero_point2 = 20;

  const auto out_cpu = at::quantize_per_tensor(
      in_cpu, scale, zero_point, c10::ScalarType::QUInt8);
  const auto out_cpu2 = at::quantize_per_tensor(
      in_cpu2, scale2, zero_point2, c10::ScalarType::QUInt8);
  const auto out_vulkan = at::native::vulkan::ops::quantize_per_tensor(
      in_vulkan, scale, zero_point, c10::ScalarType::QUInt8);
  const auto out_vulkan2 = at::native::vulkan::ops::quantize_per_tensor(
      in_vulkan2, scale2, zero_point2, c10::ScalarType::QUInt8);

  auto out_vulkan_to_cpu = at::native::int_repr_quantized_cpu(vulkan_to_cpu(out_vulkan, out_cpu));
  auto out_vulkan_to_cpu2 = at::native::int_repr_quantized_cpu(vulkan_to_cpu(out_vulkan2, out_cpu2));

  const auto reg_added_tensors = at::add(in_cpu, in_cpu2);

  const double scale3 = 0.15;
  const int zero_point3 = 15;
  const auto vulk_added_tensors = at::native::vulkan::ops::quantized_add(out_vulkan, out_vulkan2, scale3, zero_point3);

  const auto out_vulkan_deq = at::native::vulkan::ops::dequantize(vulk_added_tensors);
  auto output_for_dequantized_vulkan = vulkan_to_cpu(out_vulkan_deq, in_cpu2);

  float rtol = 0;
  float atol = 0.5;
  const auto check = at::allclose(reg_added_tensors, output_for_dequantized_vulkan, rtol, atol);

  if (!check) {
    std::cout << "Max Diff allowed: " << rtol << std::endl;
  }

  ASSERT_TRUE(check);
}

TEST_F(VulkanAPITest, conv2d) {
  if (!at::is_vulkan_available()) {
    return;
  }

  constexpr int64_t groups = 1;
  constexpr std::array<int64_t, 2u> stride{2, 2};
  constexpr std::array<int64_t, 2u> padding{1, 1};
  //TODO: Support conv2d with dilation != 1
  constexpr std::array<int64_t, 2u> dilation{1, 1};

  constexpr struct {
    uint32_t batches;
    uint32_t channels;
    uint32_t width;
    uint32_t height;

    std::array<int64_t, 4u> size() const {
      return {
        batches,
        channels,
        width,
        height,
      };
    }
  } input {1, 3, 8, 8};

  constexpr struct {
    uint32_t output_channels;
    uint32_t input_channels;
    uint32_t width;
    uint32_t height;

    std::array<int64_t, 4u> size() const {
      return {
        output_channels,
        input_channels,
        width,
        height,
      };
    }
  } weights {1, input.channels, 3, 3};

  float r1 = 0.1;
  float r2 = 0.7;
  const auto input_cpu = (r1 - r2) * at::rand(input.size(), at::device(at::kCPU).dtype(at::kFloat)) + r2;
  const auto weights_cpu = (r1 - r2) * at::rand(weights.size(), at::device(at::kCPU).dtype(at::kFloat)) + r2;
  const auto bias_cpu = (r1 - r2) * at::rand({weights.output_channels}, at::device(at::kCPU).dtype(at::kFloat)) + r2;

  const auto output_cpu = at::conv2d(
      input_cpu,
      weights_cpu,
      bias_cpu,
      stride,
      padding,
      dilation,
      groups);


  const double scale = 0.10;
  const int zero_point = 10;
  const auto shape_match =
      at::rand({1, 1, 4, 4}, at::device(at::kCPU).dtype(at::kFloat)) * 6;
  const auto in_vulkan = input_cpu.vulkan();
  const auto out_vulkan = at::native::vulkan::ops::quantize_per_tensor(
      in_vulkan, scale, zero_point, c10::ScalarType::QUInt8);

  const double scale2 = 0.15;
  const int zero_point2 = 15;
  const auto output_vulkan = at::native::vulkan::ops::conv2d(
      out_vulkan,
      weights_cpu,
      bias_cpu,
      stride,
      padding,
      dilation,
      groups,
      scale2,
      zero_point2);

  const auto out_vulkan_deq = at::native::vulkan::ops::dequantize(output_vulkan);
  auto output_for_dequantized_vulkan = vulkan_to_cpu(out_vulkan_deq, shape_match);

  float rtol = 0;
  float atol = 1;
  const auto check = at::allclose(output_cpu, output_for_dequantized_vulkan, rtol, atol);

  if (!check) {
    std::cout << "Max Diff allowed: " << rtol << std::endl;
  }

  ASSERT_TRUE(check);
}

} // namespace

#endif /* USE_VULKAN_API */
