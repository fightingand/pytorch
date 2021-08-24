
// Copyright (c) Facebook, Inc. and its affiliates.
// All rights reserved.
//
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree.

#include <functorch/csrc/BatchRulesHelper.h>
#include <ATen/Operators.h>
#include <functorch/csrc/PlumbingHelper.h>
#include <functorch/csrc/BatchedFallback.h>
#include <ATen/core/dispatch/Dispatcher.h>

namespace at { namespace functorch {

#define STOP_DECOMPOSE(op) \
  m.impl(#op, torch::CppFunction::makeFromBoxedFunction<&batchedTensorForLoopFallback>());

TORCH_LIBRARY_IMPL(functorch, FT_BATCHED_KEY, m) {
  STOP_DECOMPOSE(new_zeros_hack);
  STOP_DECOMPOSE(new_empty_hack);
}
TORCH_LIBRARY_IMPL(aten, FT_BATCHED_KEY, m) {
  // These just aren't tested, so we don't want to decompose them in case they break.
  STOP_DECOMPOSE(__and__.Scalar);
  STOP_DECOMPOSE(__and__.Tensor);
  STOP_DECOMPOSE(__iand__.Scalar);
  STOP_DECOMPOSE(__iand__.Tensor);
  STOP_DECOMPOSE(__ior__.Scalar);
  STOP_DECOMPOSE(__ior__.Tensor);
  STOP_DECOMPOSE(__ixor__.Scalar);
  STOP_DECOMPOSE(__ixor__.Tensor);
  STOP_DECOMPOSE(__or__.Scalar);
  STOP_DECOMPOSE(__or__.Tensor);
  STOP_DECOMPOSE(__xor__.Scalar);
  STOP_DECOMPOSE(__xor__.Tensor);
  STOP_DECOMPOSE(_add_batch_dim);
  STOP_DECOMPOSE(_add_relu.Scalar);
  STOP_DECOMPOSE(_add_relu_.Scalar);
  STOP_DECOMPOSE(_backward);
  STOP_DECOMPOSE(_baddbmm_mkl_);
  STOP_DECOMPOSE(_batch_norm_impl_index);
  STOP_DECOMPOSE(_batch_norm_impl_index_backward);
  STOP_DECOMPOSE(_cast_Byte);
  STOP_DECOMPOSE(_cast_Char);
  STOP_DECOMPOSE(_cast_Double);
  STOP_DECOMPOSE(_cast_Float);
  STOP_DECOMPOSE(_cast_Half);
  STOP_DECOMPOSE(_cast_Int);
  STOP_DECOMPOSE(_cast_Long);
  STOP_DECOMPOSE(_cast_Short);
  STOP_DECOMPOSE(_choose_qparams_per_tensor);
  STOP_DECOMPOSE(_conv_depthwise2d);
  STOP_DECOMPOSE(_conv_depthwise2d.out);
  STOP_DECOMPOSE(_conv_depthwise2d_backward.grad_input);
  STOP_DECOMPOSE(_conv_depthwise2d_backward.output_mask);
  STOP_DECOMPOSE(_convert_indices_from_coo_to_csr);
  STOP_DECOMPOSE(_convert_indices_from_coo_to_csr.out);
  STOP_DECOMPOSE(_convolution);
  STOP_DECOMPOSE(_convolution.deprecated);
  STOP_DECOMPOSE(_convolution_double_backward);
  STOP_DECOMPOSE(_convolution_mode);
  STOP_DECOMPOSE(_convolution_nogroup);
  STOP_DECOMPOSE(_cufft_clear_plan_cache);
  STOP_DECOMPOSE(_cufft_get_plan_cache_max_size);
  STOP_DECOMPOSE(_cufft_get_plan_cache_size);
  STOP_DECOMPOSE(_cufft_set_plan_cache_max_size);
  STOP_DECOMPOSE(_debug_has_internal_overlap);
  STOP_DECOMPOSE(_det_lu_based_helper_backward_helper);
  STOP_DECOMPOSE(_dim_arange);
  STOP_DECOMPOSE(_embedding_bag_backward);
  STOP_DECOMPOSE(_embedding_bag_sparse_backward);
  STOP_DECOMPOSE(_fake_quantize_learnable_per_channel_affine_backward);
  STOP_DECOMPOSE(_fake_quantize_learnable_per_tensor_affine_backward);
  STOP_DECOMPOSE(_fake_quantize_per_tensor_affine_cachemask_tensor_qparams);
  STOP_DECOMPOSE(_fused_moving_avg_obs_fq_helper);
  STOP_DECOMPOSE(_gather_sparse_backward);
  STOP_DECOMPOSE(_grid_sampler_2d_cpu_fallback_backward);
  STOP_DECOMPOSE(_has_compatible_shallow_copy_type);
  STOP_DECOMPOSE(_log_softmax.out);
  STOP_DECOMPOSE(_log_softmax_backward_data.out);
  STOP_DECOMPOSE(_make_dual);
  STOP_DECOMPOSE(_neg_view);
  STOP_DECOMPOSE(_nnpack_available);
  STOP_DECOMPOSE(_nnpack_spatial_convolution_backward);
  STOP_DECOMPOSE(_nnpack_spatial_convolution_backward_input);
  STOP_DECOMPOSE(_nnpack_spatial_convolution_backward_weight);
  STOP_DECOMPOSE(_pack_padded_sequence_backward);
  STOP_DECOMPOSE(_pad_packed_sequence);
  STOP_DECOMPOSE(_pin_memory);
  STOP_DECOMPOSE(_remove_batch_dim);
  STOP_DECOMPOSE(_reshape_from_tensor);
  STOP_DECOMPOSE(_rowwise_prune);
  STOP_DECOMPOSE(_saturate_weight_to_fp16);
  STOP_DECOMPOSE(_shape_as_tensor);
  STOP_DECOMPOSE(_sobol_engine_draw);
  STOP_DECOMPOSE(_sobol_engine_ff_);
  STOP_DECOMPOSE(_sobol_engine_initialize_state_);
  STOP_DECOMPOSE(_sobol_engine_scramble_);
  STOP_DECOMPOSE(_softmax.out);
  STOP_DECOMPOSE(_softmax_backward_data.out);
  STOP_DECOMPOSE(_sparse_coo_tensor_unsafe);
  STOP_DECOMPOSE(_sparse_csr_tensor_unsafe);
  STOP_DECOMPOSE(_sparse_log_softmax.Dimname);
  STOP_DECOMPOSE(_sparse_log_softmax.int);
  STOP_DECOMPOSE(_sparse_mm);
  STOP_DECOMPOSE(_sparse_softmax.Dimname);
  STOP_DECOMPOSE(_sparse_softmax.int);
  STOP_DECOMPOSE(_sparse_sum);
  STOP_DECOMPOSE(_sparse_sum.dim_dtype);
  STOP_DECOMPOSE(_sparse_sum.dtype);
  STOP_DECOMPOSE(_test_ambiguous_defaults.a);
  STOP_DECOMPOSE(_test_ambiguous_defaults.b);
  STOP_DECOMPOSE(_test_serialization_subcmul);
  STOP_DECOMPOSE(_test_string_default);
  STOP_DECOMPOSE(_thnn_differentiable_gru_cell_backward);
  STOP_DECOMPOSE(_thnn_differentiable_lstm_cell_backward);
  STOP_DECOMPOSE(_to_cpu);
  STOP_DECOMPOSE(_unpack_dual);
  STOP_DECOMPOSE(_use_cudnn_rnn_flatten_weight);
  STOP_DECOMPOSE(_validate_sparse_coo_tensor_args);
  STOP_DECOMPOSE(_validate_sparse_csr_tensor_args);
  STOP_DECOMPOSE(_version);
  STOP_DECOMPOSE(_weight_norm);
  STOP_DECOMPOSE(_weight_norm_differentiable_backward);
  STOP_DECOMPOSE(absolute);
  STOP_DECOMPOSE(absolute.out);
  STOP_DECOMPOSE(absolute_);
  STOP_DECOMPOSE(adaptive_max_pool1d);
  STOP_DECOMPOSE(addr.out);
  STOP_DECOMPOSE(affine_grid_generator_backward);
  STOP_DECOMPOSE(align_as);
  STOP_DECOMPOSE(align_tensors);
  STOP_DECOMPOSE(align_to);
  STOP_DECOMPOSE(align_to.ellipsis_idx);
  STOP_DECOMPOSE(all.dimname);
  STOP_DECOMPOSE(all.dimname_out);
  STOP_DECOMPOSE(allclose);
  STOP_DECOMPOSE(alpha_dropout);
  STOP_DECOMPOSE(alpha_dropout_);
  STOP_DECOMPOSE(aminmax.out);
  STOP_DECOMPOSE(any.dimname);
  STOP_DECOMPOSE(any.dimname_out);
  STOP_DECOMPOSE(arange);
  STOP_DECOMPOSE(arange.out);
  STOP_DECOMPOSE(arange.start);
  STOP_DECOMPOSE(arange.start_step);
  STOP_DECOMPOSE(arccos);
  STOP_DECOMPOSE(arccos.out);
  STOP_DECOMPOSE(arccos_);
  STOP_DECOMPOSE(arccosh);
  STOP_DECOMPOSE(arccosh.out);
  STOP_DECOMPOSE(arccosh_);
  STOP_DECOMPOSE(arcsin);
  STOP_DECOMPOSE(arcsin.out);
  STOP_DECOMPOSE(arcsin_);
  STOP_DECOMPOSE(arcsinh);
  STOP_DECOMPOSE(arcsinh.out);
  STOP_DECOMPOSE(arcsinh_);
  STOP_DECOMPOSE(arctan);
  STOP_DECOMPOSE(arctan.out);
  STOP_DECOMPOSE(arctan_);
  STOP_DECOMPOSE(arctanh);
  STOP_DECOMPOSE(arctanh.out);
  STOP_DECOMPOSE(arctanh_);
  STOP_DECOMPOSE(argsort);
  STOP_DECOMPOSE(argsort.dimname);
  STOP_DECOMPOSE(atleast_1d);
  STOP_DECOMPOSE(atleast_1d.Sequence);
  STOP_DECOMPOSE(atleast_2d);
  STOP_DECOMPOSE(atleast_2d.Sequence);
  STOP_DECOMPOSE(atleast_3d);
  STOP_DECOMPOSE(atleast_3d.Sequence);
  STOP_DECOMPOSE(avg_pool1d);
  STOP_DECOMPOSE(bartlett_window);
  STOP_DECOMPOSE(bartlett_window.periodic);
  STOP_DECOMPOSE(bernoulli.p);
  STOP_DECOMPOSE(bilinear);
  STOP_DECOMPOSE(binary_cross_entropy_with_logits_backward);
  STOP_DECOMPOSE(bitwise_and_.Scalar);
  STOP_DECOMPOSE(bitwise_and_.Tensor);
  STOP_DECOMPOSE(bitwise_left_shift.Tensor_Scalar_out);
  STOP_DECOMPOSE(bitwise_left_shift.Tensor_out);
  STOP_DECOMPOSE(bitwise_left_shift_.Tensor);
  STOP_DECOMPOSE(bitwise_left_shift_.Tensor_Scalar);
  STOP_DECOMPOSE(bitwise_or.Scalar);
  STOP_DECOMPOSE(bitwise_or.Tensor);
  STOP_DECOMPOSE(bitwise_or_.Scalar);
  STOP_DECOMPOSE(bitwise_or_.Tensor);
  STOP_DECOMPOSE(bitwise_right_shift.Tensor_Scalar_out);
  STOP_DECOMPOSE(bitwise_right_shift.Tensor_out);
  STOP_DECOMPOSE(bitwise_right_shift_.Tensor);
  STOP_DECOMPOSE(bitwise_right_shift_.Tensor_Scalar);
  STOP_DECOMPOSE(bitwise_xor.Scalar);
  STOP_DECOMPOSE(bitwise_xor.Tensor);
  STOP_DECOMPOSE(bitwise_xor_.Scalar);
  STOP_DECOMPOSE(bitwise_xor_.Tensor);
  STOP_DECOMPOSE(blackman_window);
  STOP_DECOMPOSE(blackman_window.periodic);
  STOP_DECOMPOSE(block_diag);
  STOP_DECOMPOSE(broadcast_to);
  STOP_DECOMPOSE(can_cast);
  STOP_DECOMPOSE(cartesian_prod);
  STOP_DECOMPOSE(cat.names);
  STOP_DECOMPOSE(cat.names_out);
  STOP_DECOMPOSE(chain_matmul);
  STOP_DECOMPOSE(chain_matmul.out);
  STOP_DECOMPOSE(choose_qparams_optimized);
  STOP_DECOMPOSE(clip);
  STOP_DECOMPOSE(clip.Tensor);
  STOP_DECOMPOSE(clip.Tensor_out);
  STOP_DECOMPOSE(clip.out);
  STOP_DECOMPOSE(clip_);
  STOP_DECOMPOSE(clip_.Tensor);
  STOP_DECOMPOSE(coalesce);
  STOP_DECOMPOSE(column_stack);
  STOP_DECOMPOSE(column_stack.out);
  STOP_DECOMPOSE(combinations);
  STOP_DECOMPOSE(conv1d.padding);
  STOP_DECOMPOSE(conv2d.padding);
  STOP_DECOMPOSE(conv3d.padding);
  STOP_DECOMPOSE(conv_tbc_backward);
  STOP_DECOMPOSE(conv_transpose1d);
  STOP_DECOMPOSE(conv_transpose3d.input);
  STOP_DECOMPOSE(cosine_embedding_loss);
  STOP_DECOMPOSE(cosine_similarity);
  STOP_DECOMPOSE(ctc_loss.IntList);
  STOP_DECOMPOSE(ctc_loss.Tensor);
  STOP_DECOMPOSE(cudnn_is_acceptable);
  STOP_DECOMPOSE(cummax.dimname);
  STOP_DECOMPOSE(cummax.dimname_out);
  STOP_DECOMPOSE(cummin.dimname);
  STOP_DECOMPOSE(cummin.dimname_out);
  STOP_DECOMPOSE(cumprod.dimname);
  STOP_DECOMPOSE(cumprod.dimname_out);
  STOP_DECOMPOSE(cumprod_.dimname);
  STOP_DECOMPOSE(cumsum.dimname);
  STOP_DECOMPOSE(cumsum.dimname_out);
  STOP_DECOMPOSE(cumsum_.dimname);
  STOP_DECOMPOSE(data);
  STOP_DECOMPOSE(det);
  STOP_DECOMPOSE(diagflat);
  STOP_DECOMPOSE(diagonal.Dimname);
  STOP_DECOMPOSE(diff.out);
  STOP_DECOMPOSE(divide.Scalar);
  STOP_DECOMPOSE(divide.Scalar_mode);
  STOP_DECOMPOSE(divide.Tensor);
  STOP_DECOMPOSE(divide.Tensor_mode);
  STOP_DECOMPOSE(divide.out);
  STOP_DECOMPOSE(divide.out_mode);
  STOP_DECOMPOSE(divide_.Scalar);
  STOP_DECOMPOSE(divide_.Scalar_mode);
  STOP_DECOMPOSE(divide_.Tensor);
  STOP_DECOMPOSE(divide_.Tensor_mode);
  STOP_DECOMPOSE(dropout_);
  STOP_DECOMPOSE(dsplit.array);
  STOP_DECOMPOSE(dsplit.int);
  STOP_DECOMPOSE(dstack);
  STOP_DECOMPOSE(dstack.out);
  STOP_DECOMPOSE(embedding_backward);
  STOP_DECOMPOSE(embedding_bag);
  STOP_DECOMPOSE(embedding_bag.padding_idx);
  STOP_DECOMPOSE(embedding_sparse_backward);
  STOP_DECOMPOSE(empty.names);
  STOP_DECOMPOSE(empty.out);
  STOP_DECOMPOSE(eye);
  STOP_DECOMPOSE(eye.m);
  STOP_DECOMPOSE(fake_quantize_per_channel_affine);
  STOP_DECOMPOSE(fake_quantize_per_channel_affine_cachemask_backward);
  STOP_DECOMPOSE(fake_quantize_per_tensor_affine);
  STOP_DECOMPOSE(fake_quantize_per_tensor_affine.tensor_qparams);
  STOP_DECOMPOSE(fake_quantize_per_tensor_affine_cachemask_backward);
  STOP_DECOMPOSE(fbgemm_linear_fp16_weight);
  STOP_DECOMPOSE(fbgemm_linear_fp16_weight_fp32_activation);
  STOP_DECOMPOSE(fbgemm_linear_int8_weight);
  STOP_DECOMPOSE(fbgemm_linear_int8_weight_fp32_activation);
  STOP_DECOMPOSE(fbgemm_linear_quantize_weight);
  STOP_DECOMPOSE(fbgemm_pack_gemm_matrix_fp16);
  STOP_DECOMPOSE(fbgemm_pack_quantized_matrix);
  STOP_DECOMPOSE(fbgemm_pack_quantized_matrix.KN);
  STOP_DECOMPOSE(feature_alpha_dropout);
  STOP_DECOMPOSE(feature_alpha_dropout_);
  STOP_DECOMPOSE(feature_dropout);
  STOP_DECOMPOSE(feature_dropout_);
  STOP_DECOMPOSE(fft_fft.out);
  STOP_DECOMPOSE(fft_fft2);
  STOP_DECOMPOSE(fft_fft2.out);
  STOP_DECOMPOSE(fft_fftfreq);
  STOP_DECOMPOSE(fft_fftfreq.out);
  STOP_DECOMPOSE(fft_fftn.out);
  STOP_DECOMPOSE(fft_fftshift);
  STOP_DECOMPOSE(fft_hfft.out);
  STOP_DECOMPOSE(fft_ifft.out);
  STOP_DECOMPOSE(fft_ifft2);
  STOP_DECOMPOSE(fft_ifft2.out);
  STOP_DECOMPOSE(fft_ifftn.out);
  STOP_DECOMPOSE(fft_ifftshift);
  STOP_DECOMPOSE(fft_ihfft.out);
  STOP_DECOMPOSE(fft_irfft.out);
  STOP_DECOMPOSE(fft_irfft2);
  STOP_DECOMPOSE(fft_irfft2.out);
  STOP_DECOMPOSE(fft_irfftn.out);
  STOP_DECOMPOSE(fft_rfft.out);
  STOP_DECOMPOSE(fft_rfft2);
  STOP_DECOMPOSE(fft_rfft2.out);
  STOP_DECOMPOSE(fft_rfftfreq);
  STOP_DECOMPOSE(fft_rfftfreq.out);
  STOP_DECOMPOSE(fft_rfftn.out);
  STOP_DECOMPOSE(fill_diagonal_);
  STOP_DECOMPOSE(fix);
  STOP_DECOMPOSE(fix.out);
  STOP_DECOMPOSE(fix_);
  STOP_DECOMPOSE(flatten.DimnameList);
  STOP_DECOMPOSE(flatten.named_out_dim);
  STOP_DECOMPOSE(flatten.using_names);
  STOP_DECOMPOSE(flatten_dense_tensors);
  STOP_DECOMPOSE(float_power.Scalar_out);
  STOP_DECOMPOSE(float_power.Tensor_Scalar_out);
  STOP_DECOMPOSE(float_power.Tensor_Tensor_out);
  STOP_DECOMPOSE(float_power_.Scalar);
  STOP_DECOMPOSE(float_power_.Tensor);
  STOP_DECOMPOSE(floor_divide_.Scalar);
  STOP_DECOMPOSE(frobenius_norm);
  STOP_DECOMPOSE(frobenius_norm.out);
  STOP_DECOMPOSE(full);
  STOP_DECOMPOSE(full.names);
  STOP_DECOMPOSE(full.out);
  STOP_DECOMPOSE(fused_moving_avg_obs_fake_quant);
  STOP_DECOMPOSE(gather.dimname);
  STOP_DECOMPOSE(gather.dimname_out);
  STOP_DECOMPOSE(ger.out);
  STOP_DECOMPOSE(gradient.array);
  STOP_DECOMPOSE(gradient.scalararray);
  STOP_DECOMPOSE(gradient.scalarint);
  STOP_DECOMPOSE(gradient.scalarrayarray);
  STOP_DECOMPOSE(gradient.scalarrayint);
  STOP_DECOMPOSE(gradient.tensorarray);
  STOP_DECOMPOSE(gradient.tensorarrayint);
  STOP_DECOMPOSE(greater.Scalar);
  STOP_DECOMPOSE(greater.Scalar_out);
  STOP_DECOMPOSE(greater.Tensor);
  STOP_DECOMPOSE(greater.Tensor_out);
  STOP_DECOMPOSE(greater_.Scalar);
  STOP_DECOMPOSE(greater_.Tensor);
  STOP_DECOMPOSE(greater_equal.Scalar);
  STOP_DECOMPOSE(greater_equal.Scalar_out);
  STOP_DECOMPOSE(greater_equal.Tensor);
  STOP_DECOMPOSE(greater_equal.Tensor_out);
  STOP_DECOMPOSE(greater_equal_.Scalar);
  STOP_DECOMPOSE(greater_equal_.Tensor);
  STOP_DECOMPOSE(gru.data);
  STOP_DECOMPOSE(gru.input);
  STOP_DECOMPOSE(gru_cell);
  STOP_DECOMPOSE(hamming_window);
  STOP_DECOMPOSE(hamming_window.periodic);
  STOP_DECOMPOSE(hamming_window.periodic_alpha);
  STOP_DECOMPOSE(hamming_window.periodic_alpha_beta);
  STOP_DECOMPOSE(hann_window);
  STOP_DECOMPOSE(hann_window.periodic);
  STOP_DECOMPOSE(hinge_embedding_loss);
  STOP_DECOMPOSE(histogram.bin_ct_out);
  STOP_DECOMPOSE(histogram.bins_tensor_out);
  STOP_DECOMPOSE(hsplit.array);
  STOP_DECOMPOSE(hsplit.int);
  STOP_DECOMPOSE(hstack);
  STOP_DECOMPOSE(hstack.out);
  STOP_DECOMPOSE(index_add.dimname);
  STOP_DECOMPOSE(index_add_);
  STOP_DECOMPOSE(index_copy.dimname);
  STOP_DECOMPOSE(index_copy_.dimname);
  STOP_DECOMPOSE(index_fill.Dimname_Scalar);
  STOP_DECOMPOSE(index_fill.Dimname_Tensor);
  STOP_DECOMPOSE(index_fill_.Dimname_Scalar);
  STOP_DECOMPOSE(index_fill_.Dimname_Tensor);
  STOP_DECOMPOSE(index_select.dimname);
  STOP_DECOMPOSE(index_select.dimname_out);
  STOP_DECOMPOSE(inner.out);
  STOP_DECOMPOSE(instance_norm);
  STOP_DECOMPOSE(is_conj);
  STOP_DECOMPOSE(is_distributed);
  STOP_DECOMPOSE(is_floating_point);
  STOP_DECOMPOSE(is_inference);
  STOP_DECOMPOSE(is_leaf);
  STOP_DECOMPOSE(is_neg);
  STOP_DECOMPOSE(is_nonzero);
  STOP_DECOMPOSE(is_pinned);
  STOP_DECOMPOSE(is_signed);
  STOP_DECOMPOSE(is_vulkan_available);
  STOP_DECOMPOSE(isclose);
  STOP_DECOMPOSE(isneginf);
  STOP_DECOMPOSE(isposinf);
  STOP_DECOMPOSE(isreal);
  STOP_DECOMPOSE(istft);
  STOP_DECOMPOSE(item);
  STOP_DECOMPOSE(kaiser_window);
  STOP_DECOMPOSE(kaiser_window.beta);
  STOP_DECOMPOSE(kaiser_window.periodic);
  STOP_DECOMPOSE(kron.out);
  STOP_DECOMPOSE(kthvalue.dimname);
  STOP_DECOMPOSE(kthvalue.dimname_out);
  STOP_DECOMPOSE(layer_norm);
  STOP_DECOMPOSE(ldexp.Tensor);
  STOP_DECOMPOSE(ldexp.out);
  STOP_DECOMPOSE(ldexp_);
  STOP_DECOMPOSE(less.Scalar);
  STOP_DECOMPOSE(less.Scalar_out);
  STOP_DECOMPOSE(less.Tensor);
  STOP_DECOMPOSE(less.Tensor_out);
  STOP_DECOMPOSE(less_.Scalar);
  STOP_DECOMPOSE(less_.Tensor);
  STOP_DECOMPOSE(less_equal.Scalar);
  STOP_DECOMPOSE(less_equal.Scalar_out);
  STOP_DECOMPOSE(less_equal.Tensor);
  STOP_DECOMPOSE(less_equal.Tensor_out);
  STOP_DECOMPOSE(less_equal_.Scalar);
  STOP_DECOMPOSE(less_equal_.Tensor);
  STOP_DECOMPOSE(linalg_cholesky.out);
  STOP_DECOMPOSE(linalg_cond.out);
  STOP_DECOMPOSE(linalg_cond.p_str);
  STOP_DECOMPOSE(linalg_cond.p_str_out);
  STOP_DECOMPOSE(linalg_eigvals.out);
  STOP_DECOMPOSE(linalg_eigvalsh.out);
  STOP_DECOMPOSE(linalg_inv.out);
  STOP_DECOMPOSE(linalg_matrix_norm.out);
  STOP_DECOMPOSE(linalg_matrix_norm.str_ord_out);
  STOP_DECOMPOSE(linalg_matrix_power.out);
  STOP_DECOMPOSE(linalg_matrix_rank.out);
  STOP_DECOMPOSE(linalg_matrix_rank.out_tol_tensor);
  STOP_DECOMPOSE(linalg_matrix_rank.tol_tensor);
  STOP_DECOMPOSE(linalg_multi_dot);
  STOP_DECOMPOSE(linalg_multi_dot.out);
  STOP_DECOMPOSE(linalg_norm.ord_str);
  STOP_DECOMPOSE(linalg_norm.ord_str_out);
  STOP_DECOMPOSE(linalg_norm.out);
  STOP_DECOMPOSE(linalg_pinv.out);
  STOP_DECOMPOSE(linalg_pinv.out_rcond_tensor);
  STOP_DECOMPOSE(linalg_pinv.rcond_tensor);
  STOP_DECOMPOSE(linalg_svd.U);
  STOP_DECOMPOSE(linalg_svdvals.out);
  STOP_DECOMPOSE(linalg_tensorinv.out);
  STOP_DECOMPOSE(linalg_tensorsolve);
  STOP_DECOMPOSE(linalg_tensorsolve.out);
  STOP_DECOMPOSE(linspace);
  STOP_DECOMPOSE(log_sigmoid.out);
  STOP_DECOMPOSE(log_softmax.Dimname);
  STOP_DECOMPOSE(logcumsumexp.dimname);
  STOP_DECOMPOSE(logcumsumexp.dimname_out);
  STOP_DECOMPOSE(logical_or);
  STOP_DECOMPOSE(logical_xor);
  STOP_DECOMPOSE(logical_xor_);
  STOP_DECOMPOSE(logspace);
  STOP_DECOMPOSE(logsumexp.names);
  STOP_DECOMPOSE(logsumexp.names_out);
  STOP_DECOMPOSE(lstm.data);
  STOP_DECOMPOSE(lstm.input);
  STOP_DECOMPOSE(lstm_cell);
  STOP_DECOMPOSE(margin_ranking_loss);
  STOP_DECOMPOSE(matmul.out);
  STOP_DECOMPOSE(matrix_power);
  STOP_DECOMPOSE(matrix_power.out);
  STOP_DECOMPOSE(matrix_rank);
  STOP_DECOMPOSE(matrix_rank.tol);
  STOP_DECOMPOSE(max.names_dim);
  STOP_DECOMPOSE(max.names_dim_max);
  STOP_DECOMPOSE(max.out);
  STOP_DECOMPOSE(max_pool1d);
  STOP_DECOMPOSE(max_pool1d_with_indices);
  STOP_DECOMPOSE(max_pool3d);
  STOP_DECOMPOSE(mean.names_dim);
  STOP_DECOMPOSE(mean.names_out);
  STOP_DECOMPOSE(median.names_dim);
  STOP_DECOMPOSE(median.names_dim_values);
  STOP_DECOMPOSE(min.names_dim);
  STOP_DECOMPOSE(min.names_dim_min);
  STOP_DECOMPOSE(min.out);
  STOP_DECOMPOSE(mish_backward);
  STOP_DECOMPOSE(mkldnn_convolution_backward_input);
  STOP_DECOMPOSE(mkldnn_convolution_backward_weights);
  STOP_DECOMPOSE(mode.dimname);
  STOP_DECOMPOSE(mode.dimname_out);
  STOP_DECOMPOSE(moveaxis.int);
  STOP_DECOMPOSE(moveaxis.intlist);
  STOP_DECOMPOSE(msort.out);
  STOP_DECOMPOSE(multilabel_margin_loss);
  STOP_DECOMPOSE(multilabel_margin_loss.out);
  STOP_DECOMPOSE(multiply.Scalar);
  STOP_DECOMPOSE(multiply.Tensor);
  STOP_DECOMPOSE(multiply.out);
  STOP_DECOMPOSE(multiply_.Scalar);
  STOP_DECOMPOSE(multiply_.Tensor);
  STOP_DECOMPOSE(mvlgamma.out);
  STOP_DECOMPOSE(nanmedian.names_dim);
  STOP_DECOMPOSE(nanmedian.names_dim_values);
  STOP_DECOMPOSE(nanquantile.new_out);
  STOP_DECOMPOSE(nanquantile.new_scalar_out);
  STOP_DECOMPOSE(nanquantile.out);
  STOP_DECOMPOSE(nanquantile.scalar_out);
  STOP_DECOMPOSE(narrow.Tensor);
  STOP_DECOMPOSE(native_layer_norm);
  STOP_DECOMPOSE(negative);
  STOP_DECOMPOSE(negative.out);
  STOP_DECOMPOSE(negative_);
  STOP_DECOMPOSE(new_empty);
  STOP_DECOMPOSE(new_full);
  STOP_DECOMPOSE(new_ones);
  STOP_DECOMPOSE(new_zeros);
  STOP_DECOMPOSE(nll_loss.out);
  STOP_DECOMPOSE(nll_loss2d.out);
  STOP_DECOMPOSE(nonzero_numpy);
  STOP_DECOMPOSE(norm.names_ScalarOpt_dim);
  STOP_DECOMPOSE(norm.names_ScalarOpt_dim_dtype);
  STOP_DECOMPOSE(norm.names_dtype_out);
  STOP_DECOMPOSE(norm.names_out);
  STOP_DECOMPOSE(norm_except_dim);
  STOP_DECOMPOSE(normal.float_float);
  STOP_DECOMPOSE(normal.float_float_out);
  STOP_DECOMPOSE(not_equal.Scalar);
  STOP_DECOMPOSE(not_equal.Scalar_out);
  STOP_DECOMPOSE(not_equal.Tensor);
  STOP_DECOMPOSE(not_equal.Tensor_out);
  STOP_DECOMPOSE(not_equal_.Scalar);
  STOP_DECOMPOSE(not_equal_.Tensor);
  STOP_DECOMPOSE(nuclear_norm.dim_out);
  STOP_DECOMPOSE(nuclear_norm.out);
  STOP_DECOMPOSE(ones);
  STOP_DECOMPOSE(ones.names);
  STOP_DECOMPOSE(ones.out);
  STOP_DECOMPOSE(orgqr);
  STOP_DECOMPOSE(orgqr.out);
  STOP_DECOMPOSE(outer.out);
  STOP_DECOMPOSE(output_nr);
  STOP_DECOMPOSE(pad_sequence);
  STOP_DECOMPOSE(pairwise_distance);
  STOP_DECOMPOSE(pdist);
  STOP_DECOMPOSE(pin_memory);
  STOP_DECOMPOSE(pixel_shuffle);
  STOP_DECOMPOSE(pixel_unshuffle);
  STOP_DECOMPOSE(poisson_nll_loss);
  STOP_DECOMPOSE(positive);
  STOP_DECOMPOSE(prod.Dimname_out);
  STOP_DECOMPOSE(prod.dim_Dimname);
  STOP_DECOMPOSE(promote_types);
  STOP_DECOMPOSE(qr.Q);
  STOP_DECOMPOSE(quantile.new_out);
  STOP_DECOMPOSE(quantile.new_scalar_out);
  STOP_DECOMPOSE(quantile.out);
  STOP_DECOMPOSE(quantile.scalar_out);
  STOP_DECOMPOSE(quantized_gru_cell);
  STOP_DECOMPOSE(quantized_lstm_cell);
  STOP_DECOMPOSE(quantized_rnn_relu_cell);
  STOP_DECOMPOSE(quantized_rnn_tanh_cell);
  STOP_DECOMPOSE(rand);
  STOP_DECOMPOSE(rand.generator);
  STOP_DECOMPOSE(rand.generator_out);
  STOP_DECOMPOSE(rand.generator_with_names);
  STOP_DECOMPOSE(rand.names);
  STOP_DECOMPOSE(rand.out);
  STOP_DECOMPOSE(randint);
  STOP_DECOMPOSE(randint.generator);
  STOP_DECOMPOSE(randint.generator_out);
  STOP_DECOMPOSE(randint.low);
  STOP_DECOMPOSE(randint.low_generator);
  STOP_DECOMPOSE(randint.low_generator_out);
  STOP_DECOMPOSE(randint.low_out);
  STOP_DECOMPOSE(randint.out);
  STOP_DECOMPOSE(randint_like);
  STOP_DECOMPOSE(randint_like.low_dtype);
  STOP_DECOMPOSE(randn);
  STOP_DECOMPOSE(randn.generator);
  STOP_DECOMPOSE(randn.generator_out);
  STOP_DECOMPOSE(randn.generator_with_names);
  STOP_DECOMPOSE(randn.names);
  STOP_DECOMPOSE(randn.out);
  STOP_DECOMPOSE(randperm);
  STOP_DECOMPOSE(randperm.generator);
  STOP_DECOMPOSE(randperm.out);
  STOP_DECOMPOSE(range);
  STOP_DECOMPOSE(range.step);
  STOP_DECOMPOSE(ravel);
  STOP_DECOMPOSE(refine_names);
  STOP_DECOMPOSE(reflection_pad3d.out);
  STOP_DECOMPOSE(reflection_pad3d_backward.grad_input);
  STOP_DECOMPOSE(relu6_);
  STOP_DECOMPOSE(rename);
  STOP_DECOMPOSE(rename_);
  STOP_DECOMPOSE(repeat_interleave.self_Tensor);
  STOP_DECOMPOSE(repeat_interleave.self_int);
  STOP_DECOMPOSE(requires_grad_);
  STOP_DECOMPOSE(resolve_conj);
  STOP_DECOMPOSE(resolve_neg);
  STOP_DECOMPOSE(retain_grad);
  STOP_DECOMPOSE(retains_grad);
  STOP_DECOMPOSE(rnn_relu.data);
  STOP_DECOMPOSE(rnn_relu.input);
  STOP_DECOMPOSE(rnn_relu_cell);
  STOP_DECOMPOSE(rnn_tanh.data);
  STOP_DECOMPOSE(rnn_tanh.input);
  STOP_DECOMPOSE(rnn_tanh_cell);
  STOP_DECOMPOSE(row_stack);
  STOP_DECOMPOSE(row_stack.out);
  STOP_DECOMPOSE(rrelu);
  STOP_DECOMPOSE(rrelu_);
  STOP_DECOMPOSE(scalar_tensor);
  STOP_DECOMPOSE(scatter.dimname_src);
  STOP_DECOMPOSE(scatter.dimname_value);
  STOP_DECOMPOSE(scatter_add.dimname);
  STOP_DECOMPOSE(select.Dimname);
  STOP_DECOMPOSE(selu_);
  STOP_DECOMPOSE(set_data);
  STOP_DECOMPOSE(silu_backward);
  STOP_DECOMPOSE(silu_backward.grad_input);
  STOP_DECOMPOSE(size.Dimname);
  STOP_DECOMPOSE(slow_conv3d);
  STOP_DECOMPOSE(slow_conv3d.out);
  STOP_DECOMPOSE(smm);
  STOP_DECOMPOSE(softmax.Dimname);
  STOP_DECOMPOSE(sort.dimname);
  STOP_DECOMPOSE(sort.dimname_stable);
  STOP_DECOMPOSE(sort.dimname_values);
  STOP_DECOMPOSE(sort.dimname_values_stable);
  STOP_DECOMPOSE(sparse_coo_tensor.indices);
  STOP_DECOMPOSE(sparse_coo_tensor.indices_size);
  STOP_DECOMPOSE(sparse_coo_tensor.size);
  STOP_DECOMPOSE(sparse_csr_tensor.crow_col_value);
  STOP_DECOMPOSE(sparse_csr_tensor.crow_col_value_size);
  STOP_DECOMPOSE(special_digamma.out);
  STOP_DECOMPOSE(special_erf.out);
  STOP_DECOMPOSE(special_erfc.out);
  STOP_DECOMPOSE(special_erfcx.out);
  STOP_DECOMPOSE(special_erfinv.out);
  STOP_DECOMPOSE(special_exp2.out);
  STOP_DECOMPOSE(special_expit.out);
  STOP_DECOMPOSE(special_expm1.out);
  STOP_DECOMPOSE(special_gammaln.out);
  STOP_DECOMPOSE(special_i0.out);
  STOP_DECOMPOSE(special_log1p.out);
  STOP_DECOMPOSE(special_log_softmax);
  STOP_DECOMPOSE(special_logit);
  STOP_DECOMPOSE(special_logit.out);
  STOP_DECOMPOSE(special_logsumexp);
  STOP_DECOMPOSE(special_logsumexp.out);
  STOP_DECOMPOSE(special_multigammaln);
  STOP_DECOMPOSE(special_multigammaln.out);
  STOP_DECOMPOSE(special_ndtr.out);
  STOP_DECOMPOSE(special_ndtri.out);
  STOP_DECOMPOSE(special_polygamma.out);
  STOP_DECOMPOSE(special_psi.out);
  STOP_DECOMPOSE(special_round.out);
  STOP_DECOMPOSE(special_sinc.out);
  STOP_DECOMPOSE(special_xlogy.other_scalar_out);
  STOP_DECOMPOSE(special_xlogy.out);
  STOP_DECOMPOSE(special_xlogy.self_scalar_out);
  STOP_DECOMPOSE(special_zeta.other_scalar_out);
  STOP_DECOMPOSE(special_zeta.out);
  STOP_DECOMPOSE(special_zeta.self_scalar_out);
  STOP_DECOMPOSE(square_);
  STOP_DECOMPOSE(squeeze.dimname);
  STOP_DECOMPOSE(squeeze_.dimname);
  STOP_DECOMPOSE(sspaddmm);
  STOP_DECOMPOSE(std.correction_names);
  STOP_DECOMPOSE(std.correction_names_out);
  STOP_DECOMPOSE(std.names_dim);
  STOP_DECOMPOSE(std.names_out);
  STOP_DECOMPOSE(std.out);
  STOP_DECOMPOSE(std_mean.correction_names);
  STOP_DECOMPOSE(std_mean.names_dim);
  STOP_DECOMPOSE(stft);
  STOP_DECOMPOSE(stride.Dimname);
  STOP_DECOMPOSE(stride.int);
  STOP_DECOMPOSE(subtract.Scalar);
  STOP_DECOMPOSE(subtract.Tensor);
  STOP_DECOMPOSE(subtract.out);
  STOP_DECOMPOSE(subtract_.Scalar);
  STOP_DECOMPOSE(subtract_.Tensor);
  STOP_DECOMPOSE(sum.DimnameList_out);
  STOP_DECOMPOSE(sum.dim_DimnameList);
  STOP_DECOMPOSE(sum_to_size);
  STOP_DECOMPOSE(svd.U);
  STOP_DECOMPOSE(swapaxes);
  STOP_DECOMPOSE(swapaxes_);
  STOP_DECOMPOSE(swapdims);
  STOP_DECOMPOSE(swapdims_);
  STOP_DECOMPOSE(take_along_dim.out);
  STOP_DECOMPOSE(tensor_split.tensor_indices_or_sections);
  STOP_DECOMPOSE(thnn_conv2d);
  STOP_DECOMPOSE(thnn_conv2d.out);
  STOP_DECOMPOSE(to_dense_backward);
  STOP_DECOMPOSE(to_mkldnn_backward);
  STOP_DECOMPOSE(transpose.Dimname);
  STOP_DECOMPOSE(triplet_margin_loss);
  STOP_DECOMPOSE(true_divide.out);
  STOP_DECOMPOSE(true_divide_.Scalar);
  STOP_DECOMPOSE(true_divide_.Tensor);
  STOP_DECOMPOSE(unbind.Dimname);
  STOP_DECOMPOSE(unflatten.Dimname);
  STOP_DECOMPOSE(unflatten.int);
  STOP_DECOMPOSE(unflatten_dense_tensors);
  STOP_DECOMPOSE(unsafe_chunk);
  STOP_DECOMPOSE(vander);
  STOP_DECOMPOSE(var.correction_names);
  STOP_DECOMPOSE(var.correction_names_out);
  STOP_DECOMPOSE(var.names_dim);
  STOP_DECOMPOSE(var.names_out);
  STOP_DECOMPOSE(var.out);
  STOP_DECOMPOSE(var_mean.correction_names);
  STOP_DECOMPOSE(var_mean.names_dim);
  STOP_DECOMPOSE(vsplit.array);
  STOP_DECOMPOSE(vsplit.int);
  STOP_DECOMPOSE(vstack);
  STOP_DECOMPOSE(vstack.out);
  STOP_DECOMPOSE(zeros);
  STOP_DECOMPOSE(zeros.names);
  STOP_DECOMPOSE(zeros.out);

  // These throw an error in our tests if we remove them
  STOP_DECOMPOSE(diag_embed);
  STOP_DECOMPOSE(index_add);
  STOP_DECOMPOSE(index_add.alpha);
  STOP_DECOMPOSE(index_copy);
  STOP_DECOMPOSE(new_empty_strided);
  STOP_DECOMPOSE(index_fill.int_Scalar);
  STOP_DECOMPOSE(index_fill.int_Tensor);
  STOP_DECOMPOSE(index_put);
  STOP_DECOMPOSE(linalg_eigvals);
  STOP_DECOMPOSE(linalg_eigvalsh);
  STOP_DECOMPOSE(linalg_matrix_norm);
  STOP_DECOMPOSE(linalg_matrix_norm.str_ord);
  STOP_DECOMPOSE(linalg_norm);
  STOP_DECOMPOSE(linalg_svdvals);
  STOP_DECOMPOSE(linalg_tensorinv);
  STOP_DECOMPOSE(masked_fill.Scalar);
  STOP_DECOMPOSE(masked_fill.Tensor);
  STOP_DECOMPOSE(masked_scatter);
  STOP_DECOMPOSE(nanquantile);
  STOP_DECOMPOSE(nanquantile.new);
  STOP_DECOMPOSE(nanquantile.new_scalar);
  STOP_DECOMPOSE(nanquantile.scalar);
  STOP_DECOMPOSE(conv_transpose2d.input);
  STOP_DECOMPOSE(frobenius_norm.dim);
  STOP_DECOMPOSE(nuclear_norm);
  STOP_DECOMPOSE(nuclear_norm.dim);
  STOP_DECOMPOSE(put);
  STOP_DECOMPOSE(quantile);
  STOP_DECOMPOSE(quantile.new);
  STOP_DECOMPOSE(quantile.new_scalar);
  STOP_DECOMPOSE(quantile.scalar);
  STOP_DECOMPOSE(take_along_dim);
  STOP_DECOMPOSE(fft_fftn);
  STOP_DECOMPOSE(fft_hfft);
  STOP_DECOMPOSE(fft_ifftn);
  STOP_DECOMPOSE(linalg_cholesky);
  STOP_DECOMPOSE(linalg_cholesky_ex);
  STOP_DECOMPOSE(linalg_inv);
  STOP_DECOMPOSE(linalg_inv_ex);
  STOP_DECOMPOSE(linalg_matrix_power);
  STOP_DECOMPOSE(linalg_matrix_rank);
  STOP_DECOMPOSE(matrix_exp);
  STOP_DECOMPOSE(logical_not);
  STOP_DECOMPOSE(logical_and);
  STOP_DECOMPOSE(kthvalue);
  STOP_DECOMPOSE(masked_select);
  STOP_DECOMPOSE(masked_select_backward);
  STOP_DECOMPOSE(matrix_exp_backward);
  STOP_DECOMPOSE(trace_backward);
  STOP_DECOMPOSE(slice_backward);
  STOP_DECOMPOSE(select_backward);
  STOP_DECOMPOSE(diagonal_backward);
  STOP_DECOMPOSE(corrcoef);
  STOP_DECOMPOSE(cummaxmin_backward);
  STOP_DECOMPOSE(cumprod_backward);
  STOP_DECOMPOSE(diag_backward);
  STOP_DECOMPOSE(logical_and_);
  STOP_DECOMPOSE(logical_or_);
  STOP_DECOMPOSE(index_select_backward);
  STOP_DECOMPOSE(value_selecting_reduction_backward);
  STOP_DECOMPOSE(infinitely_differentiable_gelu_backward);
}

}}
