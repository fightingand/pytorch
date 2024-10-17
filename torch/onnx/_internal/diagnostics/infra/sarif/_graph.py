# DO NOT EDIT! This file was generated by jschema_to_python version 0.0.1.dev29,
# with extension for dataclasses and type annotation.

from __future__ import annotations

import dataclasses
from typing import List, Optional

from torch.onnx._internal.diagnostics.infra.sarif import (
    _edge,
    _message,
    _node,
    _property_bag,
)


@dataclasses.dataclass
class Graph(object):
    """A network of nodes and directed edges that describes some aspect of the structure of the code (for example, a call graph)."""

    description: Optional[_message.Message] = dataclasses.field(
        default=None, metadata={"schema_property_name": "description"}
    )
    edges: Optional[List[_edge.Edge]] = dataclasses.field(
        default=None, metadata={"schema_property_name": "edges"}
    )
    nodes: Optional[List[_node.Node]] = dataclasses.field(
        default=None, metadata={"schema_property_name": "nodes"}
    )
    properties: Optional[_property_bag.PropertyBag] = dataclasses.field(
        default=None, metadata={"schema_property_name": "properties"}
    )


# flake8: noqa
