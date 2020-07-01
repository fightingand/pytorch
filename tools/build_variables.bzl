# In both open-source and fbcode builds, these are generated into
# torch/csrc/{autgrad,jit}/generated.i
GENERATED_CPP = [
    "autograd/generated/Functions.cpp",
    "autograd/generated/VariableType_0.cpp",
    "autograd/generated/VariableType_1.cpp",
    "autograd/generated/VariableType_2.cpp",
    "autograd/generated/VariableType_3.cpp",
    "autograd/generated/VariableType_4.cpp",
    "jit/generated/generated_unboxing_wrappers_0.cpp",
    "jit/generated/generated_unboxing_wrappers_1.cpp",
    "jit/generated/generated_unboxing_wrappers_2.cpp",
    "autograd/generated/ProfiledType_0.cpp",
    "autograd/generated/ProfiledType_1.cpp",
    "autograd/generated/ProfiledType_2.cpp",
    "autograd/generated/ProfiledType_3.cpp",
    "autograd/generated/ProfiledType_4.cpp",
    "autograd/generated/TraceType_0.cpp",
    "autograd/generated/TraceType_1.cpp",
    "autograd/generated/TraceType_2.cpp",
    "autograd/generated/TraceType_3.cpp",
    "autograd/generated/TraceType_4.cpp",
    "autograd/generated/python_functions.cpp",
    "autograd/generated/python_nn_functions.cpp",
    "autograd/generated/python_torch_functions.cpp",
    "autograd/generated/python_variable_methods.cpp",
]

libtorch_generated_sources = [
    ":generate-code=autograd/generated/Functions.cpp",
    ":generate-code=jit/generated/generated_unboxing_wrappers_0.cpp",
    ":generate-code=jit/generated/generated_unboxing_wrappers_1.cpp",
    ":generate-code=jit/generated/generated_unboxing_wrappers_2.cpp",
    ":generate-code=autograd/generated/VariableType_0.cpp",
    ":generate-code=autograd/generated/VariableType_1.cpp",
    ":generate-code=autograd/generated/VariableType_2.cpp",
    ":generate-code=autograd/generated/VariableType_3.cpp",
    ":generate-code=autograd/generated/VariableType_4.cpp",
    ":generate-code=autograd/generated/ProfiledType_0.cpp",
    ":generate-code=autograd/generated/ProfiledType_1.cpp",
    ":generate-code=autograd/generated/ProfiledType_2.cpp",
    ":generate-code=autograd/generated/ProfiledType_3.cpp",
    ":generate-code=autograd/generated/ProfiledType_4.cpp",
    ":generate-code=autograd/generated/TraceType_0.cpp",
    ":generate-code=autograd/generated/TraceType_1.cpp",
    ":generate-code=autograd/generated/TraceType_2.cpp",
    ":generate-code=autograd/generated/TraceType_3.cpp",
    ":generate-code=autograd/generated/TraceType_4.cpp",
    "torch/csrc/autograd/VariableTypeManual.cpp",
]

# copied from https://github.com/pytorch/pytorch/blob/master/aten/src/ATen/core/CMakeLists.txt
jit_core_headers = [
    "torch/csrc/utils/memory.h",
    "torch/csrc/WindowsTorchApiMacro.h",
    "torch/csrc/jit/frontend/source_range.h",
    "torch/csrc/jit/serialization/source_range_serialization.h",
    "torch/csrc/jit/frontend/lexer.h",
    "torch/csrc/jit/frontend/strtod.h",
    "torch/csrc/jit/frontend/parser_constants.h",
    "torch/csrc/jit/frontend/function_schema_parser.h",
    "torch/csrc/jit/frontend/parse_string_literal.h",
    "torch/csrc/jit/frontend/schema_type_parser.h",
    "torch/csrc/jit/frontend/error_report.h",
    "torch/csrc/jit/frontend/tree.h",
    "torch/custom_class.h",
    "torch/custom_class_detail.h",
    "torch/library.h",
]

jit_core_sources = [
    "torch/csrc/jit/frontend/error_report.cpp",
    "torch/csrc/jit/frontend/function_schema_parser.cpp",
    "torch/csrc/jit/frontend/lexer.cpp",
    "torch/csrc/jit/frontend/schema_type_parser.cpp",
    "torch/csrc/jit/frontend/strtod.cpp",
    "torch/csrc/jit/frontend/source_range.cpp",
]

# copied from https://github.com/pytorch/pytorch/blob/master/tools/cpp_build/torch/CMakeLists.txt
libtorch_core_sources = [
    "torch/csrc/autograd/anomaly_mode.cpp",
    "torch/csrc/autograd/autograd.cpp",
    "torch/csrc/autograd/cpp_hook.cpp",
    "torch/csrc/autograd/custom_function.cpp",
    "torch/csrc/autograd/engine.cpp",
    "torch/csrc/autograd/function.cpp",
    "torch/csrc/autograd/function_hook.cpp",
    "torch/csrc/autograd/functions/accumulate_grad.cpp",
    "torch/csrc/autograd/functions/basic_ops.cpp",
    "torch/csrc/autograd/functions/tensor.cpp",
    "torch/csrc/autograd/functions/utils.cpp",
    "torch/csrc/autograd/input_buffer.cpp",
    "torch/csrc/autograd/profiler.cpp",
    "torch/csrc/autograd/record_function_ops.cpp",
    "torch/csrc/autograd/saved_variable.cpp",
    "torch/csrc/autograd/variable.cpp",
    "torch/csrc/jit/api/function_impl.cpp",
    "torch/csrc/jit/api/module.cpp",
    "torch/csrc/jit/api/object.cpp",
    "torch/csrc/jit/backends/backend_interface.cpp",
    "torch/csrc/jit/codegen/fuser/codegen.cpp",
    "torch/csrc/jit/codegen/fuser/compiler.cpp",
    "torch/csrc/jit/codegen/fuser/executor.cpp",
    "torch/csrc/jit/codegen/fuser/fallback.cpp",
    "torch/csrc/jit/codegen/fuser/interface.cpp",
    "torch/csrc/jit/codegen/fuser/kernel_cache.cpp",
    "torch/csrc/jit/frontend/builtin_functions.cpp",
    "torch/csrc/jit/frontend/versioned_symbols.cpp",
    "torch/csrc/jit/frontend/canonicalize_modified_loop.cpp",
    "torch/csrc/jit/frontend/convert_to_ssa.cpp",
    "torch/csrc/jit/frontend/edit_distance.cpp",
    "torch/csrc/jit/frontend/exit_transforms.cpp",
    "torch/csrc/jit/frontend/inline_loop_condition.cpp",
    "torch/csrc/jit/frontend/ir_emitter.cpp",
    "torch/csrc/jit/frontend/name_mangler.cpp",
    "torch/csrc/jit/frontend/parser.cpp",
    "torch/csrc/jit/frontend/schema_matching.cpp",
    "torch/csrc/jit/frontend/script_type_parser.cpp",
    "torch/csrc/jit/frontend/string_to_type.cpp",
    "torch/csrc/jit/frontend/sugared_value.cpp",
    "torch/csrc/jit/frontend/tracer.cpp",
    "torch/csrc/jit/ir/alias_analysis.cpp",
    "torch/csrc/jit/ir/attributes.cpp",
    "torch/csrc/jit/ir/constants.cpp",
    "torch/csrc/jit/ir/ir.cpp",
    "torch/csrc/jit/ir/irparser.cpp",
    "torch/csrc/jit/ir/node_hashing.cpp",
    "torch/csrc/jit/ir/scope.cpp",
    "torch/csrc/jit/ir/subgraph_matcher.cpp",
    "torch/csrc/jit/ir/type_hashing.cpp",
    "torch/csrc/jit/jit_log.cpp",
    "torch/csrc/jit/mobile/type_parser.cpp",
    "torch/csrc/jit/passes/bailout_graph.cpp",
    "torch/csrc/jit/passes/batch_mm.cpp",
    "torch/csrc/jit/passes/canonicalize.cpp",
    "torch/csrc/jit/passes/canonicalize_graph_fuser_ops.cpp",
    "torch/csrc/jit/passes/clear_profiling.cpp",
    "torch/csrc/jit/passes/clear_undefinedness.cpp",
    "torch/csrc/jit/passes/common_subexpression_elimination.cpp",
    "torch/csrc/jit/passes/constant_pooling.cpp",
    "torch/csrc/jit/passes/constant_propagation.cpp",
    "torch/csrc/jit/passes/create_autodiff_subgraphs.cpp",
    "torch/csrc/jit/passes/dead_code_elimination.cpp",
    "torch/csrc/jit/passes/decompose_ops.cpp",
    "torch/csrc/jit/passes/erase_number_types.cpp",
    "torch/csrc/jit/passes/fixup_trace_scope_blocks.cpp",
    "torch/csrc/jit/passes/freeze_module.cpp",
    "torch/csrc/jit/passes/fuse_linear.cpp",
    "torch/csrc/jit/passes/graph_fuser.cpp",
    "torch/csrc/jit/passes/graph_rewrite_helper.cpp",
    "torch/csrc/jit/passes/guard_elimination.cpp",
    "torch/csrc/jit/passes/inline_autodiff_subgraphs.cpp",
    "torch/csrc/jit/passes/inline_forked_closures.cpp",
    "torch/csrc/jit/passes/inliner.cpp",
    "torch/csrc/jit/passes/inplace_check.cpp",
    "torch/csrc/jit/passes/insert_guards.cpp",
    "torch/csrc/jit/passes/lift_closures.cpp",
    "torch/csrc/jit/passes/liveness.cpp",
    "torch/csrc/jit/passes/loop_unrolling.cpp",
    "torch/csrc/jit/passes/lower_grad_of.cpp",
    "torch/csrc/jit/passes/lower_tuples.cpp",
    "torch/csrc/jit/passes/normalize_ops.cpp",
    "torch/csrc/jit/passes/peephole_list_idioms.cpp",
    "torch/csrc/jit/passes/pass_manager.cpp",
    "torch/csrc/jit/passes/peephole.cpp",
    "torch/csrc/jit/passes/create_functional_graphs.cpp",
    "torch/csrc/jit/passes/prepack_folding.cpp",
    "torch/csrc/jit/passes/fold_conv_bn.cpp",
    "torch/csrc/jit/passes/remove_expands.cpp",
    "torch/csrc/jit/passes/remove_dropout.cpp",
    "torch/csrc/jit/passes/requires_grad_analysis.cpp",
    "torch/csrc/jit/passes/shape_analysis.cpp",
    "torch/csrc/jit/passes/specialize_autogradzero.cpp",
    "torch/csrc/jit/passes/subgraph_rewrite.cpp",
    "torch/csrc/jit/passes/tensorexpr_fuser.cpp",
    "torch/csrc/jit/passes/utils/memory_dag.cpp",
    "torch/csrc/jit/passes/utils/subgraph_utils.cpp",
    "torch/csrc/jit/passes/xnnpack_rewrite.cpp",
    "torch/csrc/jit/passes/quantization/helper.cpp",
    "torch/csrc/jit/passes/quantization/insert_observers.cpp",
    "torch/csrc/jit/passes/quantization/insert_quant_dequant.cpp",
    "torch/csrc/jit/passes/quantization/dedup_module_uses.cpp",
    "torch/csrc/jit/passes/quantization/finalize.cpp",
    "torch/csrc/jit/passes/quantization/fusion_passes.cpp",
    "torch/csrc/jit/python/update_graph_executor_opt.cpp",
    "torch/csrc/jit/runtime/argument_spec.cpp",
    "torch/csrc/jit/runtime/autodiff.cpp",
    "torch/csrc/jit/runtime/graph_executor.cpp",
    "torch/csrc/jit/runtime/instruction.cpp",
    "torch/csrc/jit/runtime/interpreter.cpp",
    "torch/csrc/jit/runtime/jit_exception.cpp",
    "torch/csrc/jit/runtime/logging.cpp",
    "torch/csrc/jit/runtime/operator.cpp",
    "torch/csrc/jit/runtime/print_handler.cpp",
    "torch/csrc/jit/runtime/profiling_graph_executor_impl.cpp",
    "torch/csrc/jit/runtime/profiling_record.cpp",
    "torch/csrc/jit/runtime/register_ops_utils.cpp",
    "torch/csrc/jit/runtime/symbolic_script.cpp",
    "torch/csrc/jit/runtime/vararg_functions.cpp",
    "torch/csrc/jit/serialization/import.cpp",
    "torch/csrc/jit/serialization/import_export_helpers.cpp",
    "torch/csrc/jit/serialization/import_source.cpp",
    "torch/csrc/jit/serialization/pickle.cpp",
    "torch/csrc/jit/serialization/pickler.cpp",
    "torch/csrc/jit/serialization/python_print.cpp",
    "torch/csrc/jit/serialization/source_range_serialization.cpp",
    "torch/csrc/jit/serialization/type_name_uniquer.cpp",
    "torch/csrc/jit/serialization/unpickler.cpp",
    "torch/csrc/jit/tensorexpr/bounds_inference.cpp",
    "torch/csrc/jit/tensorexpr/codegen.cpp",
    "torch/csrc/jit/tensorexpr/eval.cpp",
    "torch/csrc/jit/tensorexpr/expr.cpp",
    "torch/csrc/jit/tensorexpr/function.cpp",
    "torch/csrc/jit/tensorexpr/hash_provider.cpp",
    "torch/csrc/jit/tensorexpr/ir.cpp",
    "torch/csrc/jit/tensorexpr/ir_mutator.cpp",
    "torch/csrc/jit/tensorexpr/ir_printer.cpp",
    "torch/csrc/jit/tensorexpr/ir_simplifier.cpp",
    "torch/csrc/jit/tensorexpr/ir_visitor.cpp",
    "torch/csrc/jit/tensorexpr/kernel.cpp",
    "torch/csrc/jit/tensorexpr/llvm_codegen.cpp",
    "torch/csrc/jit/tensorexpr/llvm_jit.cpp",
    "torch/csrc/jit/tensorexpr/loopnest.cpp",
    "torch/csrc/jit/tensorexpr/mem_arena.cpp",
    "torch/csrc/jit/tensorexpr/tensor.cpp",
    "torch/csrc/jit/tensorexpr/types.cpp",
    "torch/csrc/jit/tensorexpr/unique_name_manager.cpp",
    "torch/csrc/jit/testing/file_check.cpp",
    "torch/csrc/jit/testing/hooks_for_testing.cpp",
    "torch/csrc/utils/tensor_flatten.cpp",
    "torch/csrc/utils/variadic.cpp",
]

libtorch_distributed_sources = [
    "torch/csrc/distributed/autograd/autograd.cpp",
    "torch/csrc/distributed/autograd/utils.cpp",
    "torch/csrc/distributed/autograd/context/container.cpp",
    "torch/csrc/distributed/autograd/context/context.cpp",
    "torch/csrc/distributed/autograd/engine/dist_engine.cpp",
    "torch/csrc/distributed/autograd/functions/recvrpc_backward.cpp",
    "torch/csrc/distributed/autograd/functions/sendrpc_backward.cpp",
    "torch/csrc/distributed/autograd/rpc_messages/autograd_metadata.cpp",
    "torch/csrc/distributed/autograd/rpc_messages/propagate_gradients_req.cpp",
    "torch/csrc/distributed/autograd/rpc_messages/propagate_gradients_resp.cpp",
    "torch/csrc/distributed/autograd/rpc_messages/cleanup_autograd_context_req.cpp",
    "torch/csrc/distributed/autograd/rpc_messages/cleanup_autograd_context_resp.cpp",
    "torch/csrc/distributed/autograd/rpc_messages/rpc_with_autograd.cpp",
    "torch/csrc/distributed/rpc/message.cpp",
    "torch/csrc/distributed/rpc/profiler/server_process_global_profiler.cpp",
    "torch/csrc/distributed/rpc/python_call.cpp",
    "torch/csrc/distributed/rpc/python_remote_call.cpp",
    "torch/csrc/distributed/rpc/python_resp.cpp",
    "torch/csrc/distributed/rpc/request_callback.cpp",
    "torch/csrc/distributed/rpc/rpc_agent.cpp",
    "torch/csrc/distributed/rpc/rref_context.cpp",
    "torch/csrc/distributed/rpc/rref_proto.cpp",
    "torch/csrc/distributed/rpc/rref_impl.cpp",
    "torch/csrc/distributed/rpc/script_call.cpp",
    "torch/csrc/distributed/rpc/script_remote_call.cpp",
    "torch/csrc/distributed/rpc/script_resp.cpp",
    "torch/csrc/distributed/rpc/torchscript_functions.cpp",
    "torch/csrc/distributed/rpc/types.cpp",
    "torch/csrc/distributed/rpc/utils.cpp",
]

libtorch_core_jit_sources = [
    "torch/csrc/jit/codegen/cuda/interface.cpp",
    "torch/csrc/jit/passes/lower_graph.cpp",
    "torch/csrc/jit/runtime/register_c10_ops.cpp",
    "torch/csrc/jit/runtime/register_prim_ops.cpp",
    "torch/csrc/jit/runtime/register_prim_ops_c10.cpp",
    "torch/csrc/jit/runtime/register_prim_ops_fulljit.cpp",
    "torch/csrc/jit/runtime/register_special_ops.cpp",
    "torch/csrc/jit/runtime/register_string_ops.cpp",
    "torch/csrc/jit/passes/inline_fork_wait.cpp",
    "torch/csrc/jit/passes/remove_inplace_ops.cpp",
    "torch/csrc/jit/passes/utils/check_alias_annotation.cpp",
]

libtorch_cmake_sources = libtorch_core_sources + libtorch_core_jit_sources

libtorch_extra_sources = libtorch_core_jit_sources + [
    "torch/csrc/autograd/VariableTypeManual.cpp",
    "torch/csrc/jit/api/module_save.cpp",
    "torch/csrc/jit/codegen/fuser/cpu/fused_kernel.cpp",
    "torch/csrc/jit/mobile/function.cpp",
    "torch/csrc/jit/mobile/import.cpp",
    "torch/csrc/jit/mobile/interpreter.cpp",
    "torch/csrc/jit/mobile/module.cpp",
    "torch/csrc/jit/mobile/observer.cpp",
    "torch/csrc/jit/mobile/register_mobile_autograd.cpp",
    "torch/csrc/jit/serialization/export.cpp",
    "torch/csrc/jit/serialization/export_module.cpp",
    "torch/csrc/jit/serialization/import_legacy.cpp",
    "torch/csrc/utils/byte_order.cpp",
]

libtorch_sources = libtorch_generated_sources + libtorch_core_sources + libtorch_distributed_sources + libtorch_extra_sources

libtorch_cuda_sources = [
    "torch/csrc/cuda/comm.cpp",
    "torch/csrc/cuda/nccl.cpp",
    "torch/csrc/jit/codegen/fuser/cuda/fused_kernel.cpp",
    "torch/csrc/autograd/profiler_cuda.cpp",
    "torch/csrc/autograd/functions/comm.cpp",
    "torch/csrc/jit/codegen/cuda/arith.cpp",
    "torch/csrc/jit/codegen/cuda/compute_at.cpp",
    "torch/csrc/jit/codegen/cuda/dispatch.cpp",
    "torch/csrc/jit/codegen/cuda/expr_evaluator.cpp",
    "torch/csrc/jit/codegen/cuda/fusion.cpp",
    "torch/csrc/jit/codegen/cuda/graph_fuser.cpp",
    "torch/csrc/jit/codegen/cuda/index_compute.cpp",
    "torch/csrc/jit/codegen/cuda/ir_base_nodes.cpp",
    "torch/csrc/jit/codegen/cuda/ir_cloner.cpp",
    "torch/csrc/jit/codegen/cuda/ir_graphviz.cpp",
    "torch/csrc/jit/codegen/cuda/ir_nodes.cpp",
    "torch/csrc/jit/codegen/cuda/ir_iostream.cpp",
    "torch/csrc/jit/codegen/cuda/iter_visitor.cpp",
    "torch/csrc/jit/codegen/cuda/kernel.cpp",
    "torch/csrc/jit/codegen/cuda/kernel_cache.cpp",
    "torch/csrc/jit/codegen/cuda/lower_index.cpp",
    "torch/csrc/jit/codegen/cuda/lower_loops.cpp",
    "torch/csrc/jit/codegen/cuda/lower_unroll.cpp",
    "torch/csrc/jit/codegen/cuda/lower_thread_predicate.cpp",
    "torch/csrc/jit/codegen/cuda/lower_utils.cpp",
    "torch/csrc/jit/codegen/cuda/lower_validation.cpp",
    "torch/csrc/jit/codegen/cuda/lower2device.cpp",
    "torch/csrc/jit/codegen/cuda/manager.cpp",
    "torch/csrc/jit/codegen/cuda/shape_inference.cpp",
    "torch/csrc/jit/codegen/cuda/mutator.cpp",
    "torch/csrc/jit/codegen/cuda/parser.cpp",
    "torch/csrc/jit/codegen/cuda/partition.cpp",
    "torch/csrc/jit/codegen/cuda/predicate_compute.cpp",
    "torch/csrc/jit/codegen/cuda/tensor_view.cpp",
    "torch/csrc/jit/codegen/cuda/transform_iter.cpp",
    "torch/csrc/jit/codegen/cuda/transform_replay.cpp",
    "torch/csrc/jit/codegen/cuda/transform_rfactor.cpp",
    "torch/csrc/jit/codegen/cuda/type.cpp",
    "torch/csrc/jit/codegen/cuda/utils.cpp",
    "torch/csrc/jit/codegen/cuda/register_interface.cpp",
    "torch/csrc/jit/tensorexpr/cuda_codegen.cpp",
]

torch_cpp_srcs = [
    "torch/csrc/api/src/cuda.cpp",  # this just forwards stuff, no real CUDA
    "torch/csrc/api/src/data/datasets/mnist.cpp",
    "torch/csrc/api/src/data/samplers/distributed.cpp",
    "torch/csrc/api/src/data/samplers/random.cpp",
    "torch/csrc/api/src/data/samplers/sequential.cpp",
    "torch/csrc/api/src/data/samplers/stream.cpp",
    "torch/csrc/api/src/enum.cpp",
    "torch/csrc/api/src/jit.cpp",
    "torch/csrc/api/src/serialize.cpp",
    "torch/csrc/api/src/nn/init.cpp",
    "torch/csrc/api/src/nn/module.cpp",
    "torch/csrc/api/src/nn/modules/_functions.cpp",
    "torch/csrc/api/src/nn/modules/activation.cpp",
    "torch/csrc/api/src/nn/modules/adaptive.cpp",
    "torch/csrc/api/src/nn/modules/batchnorm.cpp",
    "torch/csrc/api/src/nn/modules/normalization.cpp",
    "torch/csrc/api/src/nn/modules/instancenorm.cpp",
    "torch/csrc/api/src/nn/modules/conv.cpp",
    "torch/csrc/api/src/nn/modules/dropout.cpp",
    "torch/csrc/api/src/nn/modules/distance.cpp",
    "torch/csrc/api/src/nn/modules/embedding.cpp",
    "torch/csrc/api/src/nn/modules/fold.cpp",
    "torch/csrc/api/src/nn/modules/linear.cpp",
    "torch/csrc/api/src/nn/modules/loss.cpp",
    "torch/csrc/api/src/nn/modules/padding.cpp",
    "torch/csrc/api/src/nn/modules/pixelshuffle.cpp",
    "torch/csrc/api/src/nn/modules/pooling.cpp",
    "torch/csrc/api/src/nn/modules/rnn.cpp",
    "torch/csrc/api/src/nn/modules/upsampling.cpp",
    "torch/csrc/api/src/nn/modules/container/functional.cpp",
    "torch/csrc/api/src/nn/options/activation.cpp",
    "torch/csrc/api/src/nn/options/adaptive.cpp",
    "torch/csrc/api/src/nn/options/batchnorm.cpp",
    "torch/csrc/api/src/nn/options/conv.cpp",
    "torch/csrc/api/src/nn/options/dropout.cpp",
    "torch/csrc/api/src/nn/options/instancenorm.cpp",
    "torch/csrc/api/src/nn/options/linear.cpp",
    "torch/csrc/api/src/nn/options/normalization.cpp",
    "torch/csrc/api/src/nn/options/embedding.cpp",
    "torch/csrc/api/src/nn/options/padding.cpp",
    "torch/csrc/api/src/nn/options/pooling.cpp",
    "torch/csrc/api/src/nn/options/rnn.cpp",
    "torch/csrc/api/src/nn/options/vision.cpp",
    "torch/csrc/api/src/optim/adagrad.cpp",
    "torch/csrc/api/src/optim/adam.cpp",
    "torch/csrc/api/src/optim/lbfgs.cpp",
    "torch/csrc/api/src/optim/optimizer.cpp",
    "torch/csrc/api/src/optim/rmsprop.cpp",
    "torch/csrc/api/src/optim/serialize.cpp",
    "torch/csrc/api/src/optim/sgd.cpp",
    "torch/csrc/api/src/serialize/input-archive.cpp",
    "torch/csrc/api/src/serialize/output-archive.cpp",
]

libtorch_python_cuda_sources = [
    "torch/csrc/cuda/Event.cpp",
    "torch/csrc/cuda/Module.cpp",
    "torch/csrc/cuda/Storage.cpp",
    "torch/csrc/cuda/Stream.cpp",
    "torch/csrc/cuda/Tensor.cpp",
    "torch/csrc/cuda/python_comm.cpp",
    "torch/csrc/cuda/python_nccl.cpp",
    "torch/csrc/cuda/serialization.cpp",
    "torch/csrc/cuda/utils.cpp",
    "torch/csrc/cuda/shared/cudart.cpp",
    "torch/csrc/cuda/shared/cudnn.cpp",
    "torch/csrc/cuda/shared/nvtx.cpp",
]

libtorch_python_core_sources = [
    "torch/csrc/CudaIPCTypes.cpp",
    "torch/csrc/DataLoader.cpp",
    "torch/csrc/Device.cpp",
    "torch/csrc/Dtype.cpp",
    "torch/csrc/DynamicTypes.cpp",
    "torch/csrc/Exceptions.cpp",
    "torch/csrc/Generator.cpp",
    "torch/csrc/Layout.cpp",
    "torch/csrc/MemoryFormat.cpp",
    "torch/csrc/QScheme.cpp",
    "torch/csrc/Module.cpp",
    "torch/csrc/PtrWrapper.cpp",
    "torch/csrc/python_dimname.cpp",
    "torch/csrc/Size.cpp",
    "torch/csrc/Storage.cpp",
    "torch/csrc/TypeInfo.cpp",
    "torch/csrc/api/src/python/init.cpp",
    "torch/csrc/autograd/functions/init.cpp",
    "torch/csrc/autograd/init.cpp",
    "torch/csrc/autograd/python_anomaly_mode.cpp",
    "torch/csrc/autograd/python_cpp_function.cpp",
    "torch/csrc/autograd/python_engine.cpp",
    "torch/csrc/autograd/python_function.cpp",
    "torch/csrc/autograd/python_hook.cpp",
    "torch/csrc/autograd/python_legacy_variable.cpp",
    "torch/csrc/autograd/python_variable.cpp",
    "torch/csrc/autograd/python_variable_indexing.cpp",
    "torch/csrc/jit/backends/backend_detail.cpp",
    "torch/csrc/jit/backends/backend_init.cpp",
    "torch/csrc/jit/backends/backend_resolver.cpp",
    "torch/csrc/jit/backends/test_backend.cpp",
    "torch/csrc/jit/python/init.cpp",
    "torch/csrc/jit/passes/onnx.cpp",
    "torch/csrc/jit/passes/onnx/cast_all_constant_to_floating.cpp",
    "torch/csrc/jit/passes/onnx/constant_fold.cpp",
    "torch/csrc/jit/passes/onnx/fixup_onnx_conditionals.cpp",
    "torch/csrc/jit/passes/onnx/fixup_onnx_loop.cpp",
    "torch/csrc/jit/passes/onnx/function_substitution.cpp",
    "torch/csrc/jit/passes/onnx/helper.cpp",
    "torch/csrc/jit/passes/onnx/peephole.cpp",
    "torch/csrc/jit/passes/onnx/prepare_division_for_onnx.cpp",
    "torch/csrc/jit/passes/onnx/scalar_type_analysis.cpp",
    "torch/csrc/jit/passes/onnx/unpack_quantized_weights.cpp",
    "torch/csrc/jit/passes/onnx/prepare_inplace_ops_for_onnx.cpp",
    "torch/csrc/jit/python/python_arg_flatten.cpp",
    "torch/csrc/jit/python/python_custom_class.cpp",
    "torch/csrc/jit/python/python_interpreter.cpp",
    "torch/csrc/jit/python/python_ir.cpp",
    "torch/csrc/jit/python/python_tracer.cpp",
    "torch/csrc/jit/python/script_init.cpp",
    "torch/csrc/jit/frontend/concrete_module_type.cpp",
    "torch/csrc/jit/python/python_sugared_value.cpp",
    "torch/csrc/jit/python/python_tree_views.cpp",
    "torch/csrc/multiprocessing/init.cpp",
    "torch/csrc/onnx/init.cpp",
    "torch/csrc/serialization.cpp",
    "torch/csrc/tensor/python_tensor.cpp",
    "torch/csrc/utils/init.cpp",
    "torch/csrc/utils/throughput_benchmark.cpp",
    "torch/csrc/utils.cpp",
    "torch/csrc/utils/cuda_lazy_init.cpp",
    "torch/csrc/utils/invalid_arguments.cpp",
    "torch/csrc/utils/object_ptr.cpp",
    "torch/csrc/utils/python_arg_parser.cpp",
    "torch/csrc/utils/python_dispatch.cpp",
    "torch/csrc/utils/structseq.cpp",
    "torch/csrc/utils/tensor_apply.cpp",
    "torch/csrc/utils/tensor_dtypes.cpp",
    "torch/csrc/utils/tensor_layouts.cpp",
    "torch/csrc/utils/tensor_memoryformats.cpp",
    "torch/csrc/utils/tensor_qschemes.cpp",
    "torch/csrc/utils/tensor_list.cpp",
    "torch/csrc/utils/tensor_new.cpp",
    "torch/csrc/utils/tensor_numpy.cpp",
    "torch/csrc/utils/tensor_types.cpp",
]

libtorch_python_distributed_sources = [
    "torch/csrc/distributed/autograd/init.cpp",
    "torch/csrc/distributed/c10d/comm.cpp",
    "torch/csrc/distributed/c10d/init.cpp",
    "torch/csrc/distributed/c10d/reducer.cpp",
    "torch/csrc/distributed/rpc/init.cpp",
    "torch/csrc/distributed/rpc/process_group_agent.cpp",
    "torch/csrc/distributed/rpc/py_rref.cpp",
    "torch/csrc/distributed/rpc/python_functions.cpp",
    "torch/csrc/distributed/rpc/python_rpc_handler.cpp",
    "torch/csrc/distributed/rpc/request_callback_impl.cpp",
    "torch/csrc/distributed/rpc/tensorpipe_agent.cpp",
    "torch/csrc/distributed/rpc/testing/faulty_process_group_agent.cpp",
    "torch/csrc/distributed/rpc/testing/init.cpp",
    "torch/csrc/distributed/rpc/unpickled_python_call.cpp",
    "torch/csrc/distributed/rpc/unpickled_python_remote_call.cpp",
    "torch/csrc/jit/runtime/register_distributed_ops.cpp",
]

def glob_libtorch_python_sources():
    _libtorch_python_sources = [
        ":generate-code=autograd/generated/python_functions.cpp",
        ":generate-code=autograd/generated/python_nn_functions.cpp",
        ":generate-code=autograd/generated/python_torch_functions.cpp",
        ":generate-code=autograd/generated/python_variable_methods.cpp",
    ]

    _libtorch_python_sources.extend(libtorch_python_core_sources)
    _libtorch_python_sources.extend(libtorch_python_distributed_sources)

    _libtorch_python_sources.extend([
        "test/cpp/jit/torch_python_test.cpp",
        "test/cpp/tensorexpr/padded_buffer.cpp",
        "test/cpp/jit/test_alias_analysis.cpp",
        "test/cpp/jit/test_argument_spec.cpp",
        "test/cpp/jit/test_autodiff.cpp",
        "test/cpp/jit/test_base.cpp",
        "test/cpp/jit/test_class_import.cpp",
        "test/cpp/jit/test_class_parser.cpp",
        "test/cpp/jit/test_class_type.cpp",
        "test/cpp/jit/test_code_template.cpp",
        "test/cpp/jit/test_constant_pooling.cpp",
        "test/cpp/jit/test_create_autodiff_subgraphs.cpp",
        "test/cpp/jit/test_custom_class.cpp",
        "test/cpp/jit/test_custom_operators.cpp",
        "test/cpp/jit/test_dce.cpp",
        "test/cpp/jit/test_fuser.cpp",
        "test/cpp/jit/test_gpu.cpp",
        "test/cpp/jit/test_graph_executor.cpp",
        "test/cpp/jit/test_inliner.cpp",
        "test/cpp/jit/test_interface.cpp",
        "test/cpp/jit/test_interpreter.cpp",
        "test/cpp/jit/test_ir.cpp",
        "test/cpp/jit/test_irparser.cpp",
        "test/cpp/jit/test_jit_type.cpp",
        "test/cpp/jit/test_lite_interpreter.cpp",
        "test/cpp/jit/test_misc.cpp",
        "test/cpp/jit/test_mobile_type_parser.cpp",
        "test/cpp/jit/test_module_api.cpp",
        "test/cpp/jit/test_peephole_optimize.cpp",
        "test/cpp/jit/test_qualified_name.cpp",
        "test/cpp/jit/test_save_load.cpp",
        "test/cpp/jit/test_schema_matching.cpp",
        "test/cpp/jit/test_subgraph_matcher.cpp",
        "test/cpp/jit/test_subgraph_rewriter.cpp",
        "test/cpp/jit/test_subgraph_utils.cpp",
        "test/cpp/jit/test_utils.cpp",
    ])

    _libtorch_python_sources.extend(native.glob(["test/cpp/tensorexpr/test_*.cpp"]))

    return _libtorch_python_sources
