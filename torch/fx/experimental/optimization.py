# mypy: allow-untyped-defs
import torch.fx as fx
from torch.fx.node import Argument, Target
from torch.nn.utils.fusion import fuse_conv_bn_eval
from typing import Type, Dict, Any, Tuple, Iterable, Optional, List, cast
import torch
import torch.nn as nn
import torch.nn.functional as F
from torch.fx.passes.shape_prop import ShapeProp
import copy
from collections import defaultdict
import torch.utils.onednn as th_onednn
import operator
import time
import logging
from enum import Enum

def _parent_name(target : str) -> Tuple[str, str]:
    """
    Splits a qualname into parent path and last atom.
    For example, `foo.bar.baz` -> (`foo.bar`, `baz`)
    """
    *parent, name = target.rsplit('.', 1)
    return parent[0] if parent else '', name

# Works for length 2 patterns with 2 modules
def matches_module_pattern(pattern: Iterable[Type], node: fx.Node, modules: Dict[str, Any]):
    if len(node.args) == 0:
        return False
    nodes: Tuple[Any, fx.Node] = (node.args[0], node)
    for expected_type, current_node in zip(pattern, nodes):
        if not isinstance(current_node, fx.Node):
            return False
        if current_node.op != 'call_module':
            return False
        if not isinstance(current_node.target, str):
            return False
        if current_node.target not in modules:
            return False
        if type(modules[current_node.target]) is not expected_type:
            return False
    return True


def replace_node_module(node: fx.Node, modules: Dict[str, Any], new_module: torch.nn.Module):
    assert isinstance(node.target, str)
    parent_name, name = _parent_name(node.target)
    modules[node.target] = new_module
    setattr(modules[parent_name], name, new_module)

def fuse(model: torch.nn.Module, inplace=False, no_trace=False) -> torch.nn.Module:
    """
    Fuses convolution/BN layers for inference purposes. Will deepcopy your
    model by default, but can modify the model inplace as well.
    """
    patterns = [(nn.Conv1d, nn.BatchNorm1d),
                (nn.Conv2d, nn.BatchNorm2d),
                (nn.Conv3d, nn.BatchNorm3d)]
    if not inplace:
        model = copy.deepcopy(model)
    if not no_trace or not isinstance(model, torch.fx.GraphModule):
        fx_model = fx.symbolic_trace(model)
    else:
        fx_model = model
    modules = dict(fx_model.named_modules())
    new_graph = copy.deepcopy(fx_model.graph)

    for pattern in patterns:
        for node in new_graph.nodes:
            if matches_module_pattern(pattern, node, modules):
                if len(node.args[0].users) > 1:  # Output of conv is used by other nodes
                    continue
                conv = modules[node.args[0].target]
                bn = modules[node.target]
                if not bn.track_running_stats:
                    continue
                fused_conv = fuse_conv_bn_eval(conv, bn)
                replace_node_module(node.args[0], modules, fused_conv)
                node.replace_all_uses_with(node.args[0])
                new_graph.erase_node(node)
    return fx.GraphModule(fx_model, new_graph)

def remove_dropout(model: nn.Module) -> nn.Module:
    """
    Removes all dropout layers from the module.
    """
    fx_model = fx.symbolic_trace(model)

    class DropoutRemover(torch.fx.Transformer):
        def call_module(self, target : Target, args : Tuple[Argument, ...], kwargs : Dict[str, Any]) -> Any:
            if isinstance(self.submodules[target], nn.Dropout):
                assert len(args) == 1
                return args[0]
            else:
                return super().call_module(target, args, kwargs)
    return DropoutRemover(fx_model).transform()

def extract_subgraph(orig_module: nn.Module, nodes: List[fx.Node], inputs: List[fx.Node], outputs: List[fx.Node]):
    """
    Given lists of nodes from an existing graph that represent a subgraph, returns a submodule that executes that subgraph.
    """
    new_graph = fx.Graph()
    env: Dict[fx.Node, fx.Node] = {}
    for input in inputs:
        new_node = new_graph.placeholder(input.name)
        env[input] = new_node
    for node in nodes:
        new_node = new_graph.node_copy(node, lambda x: env[x])
        env[node] = new_node
    new_graph.output([env[output] for output in outputs])
    new_graph.lint()
    return fx.GraphModule(orig_module, new_graph)

onednn_supported = [
    nn.Conv2d, nn.Linear, nn.BatchNorm2d, nn.ReLU, nn.MaxPool2d, nn.AvgPool2d, nn.AdaptiveAvgPool2d,
    torch.relu, torch.transpose, torch.sigmoid,
    F.relu, F.avg_pool2d, F.adaptive_avg_pool2d
]
# These are operators that may not be convertible into ONEDNN ops (e.g. the
# args are scalar values). Thus, we only include them in the subgraph if their
# arguments are already in ONEDNN.
# TODO: Determine whether this can be removed after type inference.
onednn_supported_unknown = [operator.add, operator.mul]
onednn_map = {
    nn.Conv2d: th_onednn.OnednnConv2d,
    nn.Linear: th_onednn.OnednnLinear,
    nn.BatchNorm2d: lambda a, _: th_onednn.OnednnBatchNorm(a)
}


def modules_to_onednn(nodes: List[fx.Node], modules: Dict[str, nn.Module]):
    """
    For each node, if it's a module that can be preconverted into ONEDNN,
    then we do so and create a mapping to allow us to convert from the ONEDNN
    version of the module to the original.
    """
    old_modules: Dict[nn.Module, nn.Module] = {}
    for node in nodes:
        if node.op == 'call_module':
            assert isinstance(node.target, str)
            cur_module = modules[node.target]
            if type(cur_module) in onednn_map:
                new_module = onednn_map[type(cur_module)](cur_module, torch.float)
                assert isinstance(new_module, nn.Module)
                old_modules[new_module] = copy.deepcopy(cur_module)
                replace_node_module(node, modules, new_module)
    return old_modules

def reset_modules(nodes: List[fx.Node], modules: Dict[str, nn.Module], old_modules: Dict[nn.Module, nn.Module]):
    """
    Maps each module that's been changed with `modules_to_onednn` back to its
    original.
    """
    for node in nodes:
        if node.op == 'call_module':
            assert (isinstance(node.target, str))
            cur_module = modules[node.target]
            if cur_module in old_modules:
                replace_node_module(node, modules, old_modules[cur_module])

class MklSubgraph:
    def __init__(self, fx_graph: fx.Graph):
        self.fx_graph = fx_graph
        self.nodes: List[fx.Node] = []
        self.start_nodes: List[fx.Node] = []
        self.end_nodes: List[fx.Node] = []

def gen_mkl_autotuner(example_inputs, iters=10, warmup=1):
    """
    This generates a heuristic that can be passed into `optimize_for_inference` that
    determines whether a subgraph should be run in MKL by running it with the example_inputs.

    Example usage:
        heuristic = gen_mkl_autotuner(example_inputs, iters=10)
        fast_model = optimization.optimize_for_inference(model, heuristic)
    """
    fx_model = None
    old_modules = None

    def use_mkl_heuristic(graph: MklSubgraph) -> bool:
        nonlocal fx_model, old_modules
        input_nodes = graph.start_nodes
        if fx_model is None:
            fx_model = graph.fx_graph.owning_module
            old_modules = graph.fx_graph.old_modules  # type: ignore[attr-defined]
            ShapeProp(fx_model).propagate(example_inputs)
        sample_inputs = [torch.randn(node.shape) for node in input_nodes]  # type: ignore[attr-defined]
        output_args = cast(List[fx.Node], [node.args[0] for node in graph.end_nodes])
        submodule = extract_subgraph(fx_model, graph.nodes, input_nodes, output_args)

        def benchmark(f):
            for _ in range(warmup):
                f()
            begin = time.time()
            for _ in range(iters):
                out = f()
            return time.time() - begin

        mkl_time = benchmark(lambda: [i.to_dense() for i in submodule(*[i.to_onednn() for i in sample_inputs])])

        reset_modules(submodule.graph.nodes, dict(submodule.named_modules()), old_modules)
        no_mkl_time = benchmark(lambda: submodule(*sample_inputs))
        return mkl_time < no_mkl_time
    return use_mkl_heuristic

def use_mkl_length(graph: MklSubgraph) -> bool:
    """
    This is a heuristic that can be passed into `optimize_for_inference` that
    determines whether a subgraph should be run in MKL by checking if there
    are more than 2 nodes in it
    """
    return len(graph.nodes) > 2

class UnionFind:
    def __init__(self, n):
        self.parent: List[Optional[int]] = [None] * n
        self.size: List[int] = [0] * n

    def make_set(self, v: int):
        self.parent[v] = v
        self.size[v] = 1

    def find(self, v: int) -> int:
        par = self.parent[v]
        if v == par:
            return v
        assert par is not None
        self.parent[v] = self.find(par)
        return cast(int, self.parent[v])

    def join(self, a: int, b: int):
        a, b = self.find(a), self.find(b)
        if a == b:
            return a
        if self.size[a] < self.size[b]:
            a, b = b, a
        self.parent[b] = a
        self.size[a] += self.size[b]

def optimize_for_inference(
    model: torch.nn.Module,
    pass_config: Optional[Dict[str, Any]] = None,
    tracer: Type[fx.Tracer] = fx.Tracer
) -> torch.nn.Module:
    """
    Performs a set of optimization passes to optimize a model for the
    purposes of inference. Specifically, the passes that are run are:
    1. Conv/BN fusion
    2. Dropout removal
    3. MKL layout optimizations

    The third optimization takes a function `use_mkl_heuristic` that's used
    to determine whether a subgraph should be explicitly run in MKL layout.

    Note: As FX does not currently handle aliasing, this pass currently
    assumes nothing aliases. If that isn't true, use at your own risk.
    """
    default_pass_config = {
        "conv_bn_fuse": True,
        "remove_dropout": True,
        "onednn_layout_optimize": {'heuristic': use_mkl_length},
    }
    if pass_config is None:
        pass_config = {}
    default_pass_config.update(pass_config)

    if default_pass_config["conv_bn_fuse"]:
        model = fuse(model)
    if default_pass_config["remove_dropout"]:
        model = remove_dropout(model)
    if default_pass_config["onednn_layout_optimize"] is False:
        return model
    if not isinstance(default_pass_config["onednn_layout_optimize"], dict):
        raise RuntimeError("onednn_layout_optimize config is not a dict")
    if "heuristic" not in default_pass_config["onednn_layout_optimize"]:
        raise RuntimeError("Heuristic not found in onednn_layout_optimize config")
    use_mkl_heuristic = default_pass_config["onednn_layout_optimize"]["heuristic"]

    cur_tracer = tracer()
    fx_graph = cur_tracer.trace(copy.deepcopy(model))
    fx_model = fx.GraphModule(cur_tracer.root, fx_graph)
    modules: Dict[str, nn.Module] = dict(model.named_modules())

    class MklSupport(Enum):
        NO = 1
        YES = 2
        UNKNOWN = 3

    # Inserts to_onednn and to_dense around every node we want to be a ONEDNN node.
    # If the op is in `onednn_supported` then we always treat it as a ONEDNN node.
    # However, if it's in `onednn_supported_unknown`, then we only treat it as
    # a ONEDNN node if its inputs are ONEDNN nodes.
    for node in list(fx_graph.nodes):
        supports_onednn = MklSupport.NO
        if node.op == 'call_module':
            cur_module = modules[node.target]
            if type(cur_module) in onednn_supported:
                supports_onednn = MklSupport.YES
                sample_parameter = next(cur_module.parameters(), None)
                if sample_parameter is not None:
                    assert sample_parameter.dtype == torch.float, "this pass is only for torch.float modules"
                    assert sample_parameter.device == torch.device('cpu'), "this pass is only for CPU modules"
        elif node.op == 'call_function':
            if node.target in onednn_supported:
                supports_onednn = MklSupport.YES
            elif node.target in onednn_supported_unknown:
                supports_onednn = MklSupport.UNKNOWN

        if supports_onednn != MklSupport.NO:
            if supports_onednn == MklSupport.UNKNOWN:
                if not any(arg.target == 'to_dense' for arg in node.args):
                    continue
            with fx_graph.inserting_before(node):
                onednn_args = fx.map_arg(node.args, lambda n: fx_graph.call_method('to_onednn', (n, )))

            node.args = cast(Tuple[fx.node.Argument], onednn_args)

            with fx_graph.inserting_after(node):
                dense_x = fx_graph.create_node('call_method', 'to_dense', (node,))
                node.replace_all_uses_with(dense_x)
                dense_x.args = (node,)

    # Does pre-conversion of all modules into ONEDNN (when possible)
    old_modules = modules_to_onednn(list(fx_graph.nodes), modules)
    fx_graph.old_modules = old_modules  # type: ignore[attr-defined]

    # optimizes all a -> to_dense -> to_onednn -> b patterns into a -> b
    for node in fx_graph.nodes:
        if node.op == 'call_method' and node.target == 'to_dense':
            prv_node = node.args[0]
            users = list(node.users)
            for user in users:
                if user.op == 'call_method' and user.target == 'to_onednn':
                    user.replace_all_uses_with(prv_node)
                    fx_graph.erase_node(user)
            if len(node.users) == 0:
                fx_graph.erase_node(node)


    num_nodes = len(fx_graph.nodes)
    uf = UnionFind(num_nodes)

    def get_color(n):
        if hasattr(n, 'color'):  # Current node is part of a MKL subgraph
            return uf.find(n.color)
        if hasattr(n, 'start_color'):  # Current node is input to MKL subgraph
            return uf.find(n.start_color)
        return None


    # This code is to find each ONEDNN subgraph. Each ONEDNN subgraph consists
    # of input nodes (which are only `to_onednn` calls), output nodes
    # (`to_dense` calls), and intermediate nodes, which are run entirely on
    # ONEDNN layout tensors.
    #
    # Specifically, this code does a flood fill on a directed acyclic graph
    # (DAG), starting from each possible "start node" (i.e: `to_onednn` nodes).
    # If every node only had one input, this would be sufficient. However, in
    # the case that a node has multiple inputs coming from different start
    # nodes (i.e. colors), we need to join these 2 colors into 1. That's done
    # using a Disjoint Set Union.
    for cur_idx, node in enumerate(fx_graph.nodes):
        if node.op == 'call_method' and node.target == 'to_onednn':
            node.start_color = cur_idx
            uf.make_set(cur_idx)
        elif node.op == 'call_method' and node.target == 'to_dense':
            assert get_color(node.args[0]) is not None
            node.end_color = get_color(node.args[0])
        else:
            cur_colors = [get_color(i) for i in node.all_input_nodes if isinstance(i, fx.Node) if get_color(i) is not None]

            if len(cur_colors) == 0:
                continue
            assert not any(i is None for i in cur_colors)
            cur_colors = sorted(cur_colors)
            node.color = cur_colors[0]
            for other_color in cur_colors[1:]:
                uf.join(cur_colors[0], other_color)


    onednn_graphs: Dict[int, MklSubgraph] = defaultdict(lambda: MklSubgraph(fx_graph))
    for node in fx_graph.nodes:
        if hasattr(node, 'color'):
            onednn_graphs[uf.find(node.color)].nodes.append(node)
        if hasattr(node, 'start_color'):
            onednn_graphs[uf.find(node.start_color)].start_nodes.append(node)
        if hasattr(node, 'end_color'):
            onednn_graphs[uf.find(node.end_color)].end_nodes.append(node)


    # Now that we have all the subgraphs, we need to decide which ONEDNN
    # subgraphs we actually want to keep in ONEDNN.
    for graph in onednn_graphs.values():
        if not use_mkl_heuristic(graph):
            for node in graph.start_nodes + graph.end_nodes:
                prv = node.args[0]
                node.replace_all_uses_with(prv)
                fx_graph.erase_node(node)
            reset_modules(graph.nodes, modules, old_modules)

    onednn_conversions = 0
    for node in fx_graph.nodes:
        if node.target == 'to_onednn' or node.target == 'to_dense':
            onednn_conversions += 1

    logging.getLogger(__name__).info("onednn conversions: %s", onednn_conversions)
    fx_graph.lint()
    result = fx.GraphModule(model, fx_graph)
    return result
