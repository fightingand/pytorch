# mypy: allow-untyped-defs
from __future__ import annotations

import torch
import torch.export
import torch.fx
from torch.onnx._internal.exporter import _decomp, _registration
from torch.onnx._internal.fx import diagnostics, passes


def decompose_with_registry(
    exported_program: torch.export.ExportedProgram, registry: _registration.ONNXRegistry
) -> torch.export.ExportedProgram:
    """Decompose the exported program with the given registry.

    This function is needed so it shows clearly on the profiler results.
    """
    onnx_registered_ops = set(_decomp.get_onnx_implemented_overloads(registry))
    decomp_table = _decomp.create_onnx_friendly_decomposition_table(onnx_registered_ops)
    return exported_program.run_decompositions(decomp_table)


def insert_type_promotion_nodes(
    graph_module: torch.fx.GraphModule,
) -> None:
    """Inplace pass to insert explicit type promotion nodes, recursively through nested modules."""
    for module in graph_module.modules():
        assert isinstance(module, torch.fx.GraphModule)
        diagnostic_context = diagnostics.DiagnosticContext(
            "torch.onnx.export",
            torch.__version__,
        )
        passes.InsertTypePromotion(diagnostic_context, module).run()


def remove_assertion_nodes(graph_module: torch.fx.GraphModule) -> torch.fx.GraphModule:
    """Remove all assertion and check nodes from the FX graph"""
    aten_assertion_targets = {
        torch.ops.aten.sym_constrain_range_for_size.default,
        torch.ops.aten._assert_async.default,
        torch.ops.aten._assert_async.msg,
        torch.ops.aten._assert_scalar.default,
        torch.ops.aten._assert_tensor_metadata.default,
    }
    for node in graph_module.graph.nodes:
        if node.op == "call_function" and node.target in aten_assertion_targets:
            graph_module.graph.erase_node(node)
    graph_module.recompile()
    return graph_module
