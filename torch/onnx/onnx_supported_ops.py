from inspect import signature, _empty  # type: ignore[attr-defined]
from torch._C import _jit_get_all_schemas, FunctionSchema
from torch.onnx.symbolic_registry import _registry, register_version
from torch.onnx.symbolic_helper import _onnx_main_opset, _onnx_stable_opsets
from typing import Dict, List, Union


class TorchSchema:
    def __init__(self, schema: Union[FunctionSchema, str]) -> None:
        if isinstance(schema, FunctionSchema):
            self.name: str = schema.name
            self.overload_name: str = schema.overload_name
            self.arguments: List[str] = [arg.name for arg in schema.arguments]
            self.optional_arguments: List[str] = []
            self.returns: List[str] = [ret.name for ret in schema.returns]
            self.opsets: List[int] = []
        else:
            self.name = schema
            self.overload_name = ""
            self.arguments = []
            self.optional_arguments = []
            self.returns = []
            self.opsets = []

    def __str__(self) -> str:
        s = f"{self.name}.{self.overload_name}("
        s += ", ".join(self.arguments)
        s += ") -> ("
        s += ", ".join(self.returns)
        s += ")"
        s += " in opsets "
        s += ", ".join(str(opset) for opset in self.opsets)
        return s

    def __eq__(self, other) -> bool:
        if not isinstance(other, TorchSchema):
            return False
        # TODO: handle overloads?
        return self.name == other.name

    def is_aten(self) -> bool:
        return self.name.startswith("aten::")

    def is_backward(self) -> bool:
        return "backward" in self.name


def all_aten_forward_schemas():
    """ Creates a list of TorchSchema for all aten schemas"""

    torch_schemas = [TorchSchema(s) for s in _jit_get_all_schemas()]
    torch_schemas = sorted(torch_schemas, key=lambda x: x.name)
    aten_schemas = [s for s in torch_schemas if s.is_aten() and not s.is_backward()]
    return aten_schemas


# TODO: Do not hard code opset here
# Create TorchSchema object directory of all registered symbolics
# get_registered_op(opname, domain, version):
for v in _onnx_stable_opsets + [_onnx_main_opset]:
    register_version("", v)


def symbolic_argument_count(func):
    params = []
    sig = signature(func)
    optional_params = []
    has_var = False
    for name, p in sig.parameters.items():
        if p.kind.name == "VAR_POSITIONAL":
            has_var = True
        elif name == "_outputs" or name == "g":
            continue
        elif p.default != _empty:
            optional_params.append(p)
        else:
            params.append(str(p))
    return params


def all_symbolics_schemas():
    symbolics_schemas: Dict[str, TorchSchema] = dict()

    for domain, version in _registry:
        for opname, sym_func in _registry[(domain, version)].items():
            symbolics_schema = TorchSchema("aten::" + opname)
            symbolics_schema.arguments = symbolic_argument_count(sym_func)
            if opname in symbolics_schemas.keys():
                symbolics_schemas[opname].opsets.append(version)
            else:
                symbolics_schema.opsets = [version]
                symbolics_schemas[opname] = symbolics_schema
    return symbolics_schemas


def onnx_supported_ops():
    aten_schemas = all_aten_forward_schemas()
    symbolic_schemas = all_symbolics_schemas()
    supported_ops, unsupported_ops = list(), list()
    onnx_supported_ops = list()
    for schema in aten_schemas:
        if schema in symbolic_schemas.values():
            opname = schema.name[6:]  # without "aten::" prefix
            opsets = symbolic_schemas[opname].opsets
            if schema not in supported_ops:
                supported_ops.append(symbolic_schemas[opname])
                onnx_supported_ops.append((opname, " ".join([str(o) for o in opsets])))
        else:
            unsupported_ops.append(schema)
    return sorted(onnx_supported_ops, key=lambda x: x[0])
