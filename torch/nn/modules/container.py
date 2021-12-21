import warnings
from collections import OrderedDict, abc as container_abcs
from itertools import islice
import operator

import torch
from .module import Module
from torch._jit_internal import _copy_to_script_wrapper

from typing import Any, Dict, Iterable, Iterator, Mapping, Optional, TYPE_CHECKING, overload, Tuple, TypeVar, Union

if TYPE_CHECKING:
    from torch.nn import Parameter

T = TypeVar('T', bound=Module)


class Container(Module):

    def __init__(self, **kwargs: Any) -> None:
        super(Container, self).__init__()
        # DeprecationWarning is ignored by default <sigh>
        warnings.warn("nn.Container is deprecated. All of it's functionality "
                      "is now implemented in nn.Module. Subclass that instead.")
        for key, value in kwargs.items():
            self.add_module(key, value)


class Sequential(Module):
    r"""A sequential container.
    Modules will be added to it in the order they are passed in the
    constructor. Alternatively, an ``OrderedDict`` of modules can be
    passed in. The ``forward()`` method of ``Sequential`` accepts any
    input and forwards it to the first module it contains. It then
    "chains" outputs to inputs sequentially for each subsequent module,
    finally returning the output of the last module.

    The value a ``Sequential`` provides over manually calling a sequence
    of modules is that it allows treating the whole container as a
    single module, such that performing a transformation on the
    ``Sequential`` applies to each of the modules it stores (which are
    each a registered submodule of the ``Sequential``).

    What's the difference between a ``Sequential`` and a
    :class:`torch.nn.ModuleList`? A ``ModuleList`` is exactly what it
    sounds like--a list for storing ``Module`` s! On the other hand,
    the layers in a ``Sequential`` are connected in a cascading way.

    Example::

        # Using Sequential to create a small model. When `model` is run,
        # input will first be passed to `Conv2d(1,20,5)`. The output of
        # `Conv2d(1,20,5)` will be used as the input to the first
        # `ReLU`; the output of the first `ReLU` will become the input
        # for `Conv2d(20,64,5)`. Finally, the output of
        # `Conv2d(20,64,5)` will be used as input to the second `ReLU`
        model = nn.Sequential(
                  nn.Conv2d(1,20,5),
                  nn.ReLU(),
                  nn.Conv2d(20,64,5),
                  nn.ReLU()
                )

        # Using Sequential with OrderedDict. This is functionally the
        # same as the above code
        model = nn.Sequential(OrderedDict([
                  ('conv1', nn.Conv2d(1,20,5)),
                  ('relu1', nn.ReLU()),
                  ('conv2', nn.Conv2d(20,64,5)),
                  ('relu2', nn.ReLU())
                ]))
    """

    _modules: Dict[str, Module]  # type: ignore[assignment]

    @overload
    def __init__(self, *args: Module) -> None:
        ...

    @overload
    def __init__(self, arg: 'OrderedDict[str, Module]') -> None:
        ...

    def __init__(self, *args):
        super(Sequential, self).__init__()
        if len(args) == 1 and isinstance(args[0], OrderedDict):
            for key, module in args[0].items():
                self.add_module(key, module)
        else:
            for idx, module in enumerate(args):
                self.add_module(str(idx), module)

    def _get_item_by_idx(self, iterator, idx) -> T:
        """Get the idx-th item of the iterator"""
        size = len(self)
        idx = operator.index(idx)
        if not -size <= idx < size:
            raise IndexError('index {} is out of range'.format(idx))
        idx %= size
        return next(islice(iterator, idx, None))

    @_copy_to_script_wrapper
    def __getitem__(self, idx) -> Union['Sequential', T]:
        if isinstance(idx, slice):
            return self.__class__(OrderedDict(list(self._modules.items())[idx]))
        else:
            return self._get_item_by_idx(self._modules.values(), idx)

    def __setitem__(self, idx: int, module: Module) -> None:
        key: str = self._get_item_by_idx(self._modules.keys(), idx)
        return setattr(self, key, module)

    def __delitem__(self, idx: Union[slice, int]) -> None:
        if isinstance(idx, slice):
            for key in list(self._modules.keys())[idx]:
                delattr(self, key)
        else:
            key = self._get_item_by_idx(self._modules.keys(), idx)
            delattr(self, key)

    @_copy_to_script_wrapper
    def __len__(self) -> int:
        return len(self._modules)

    @_copy_to_script_wrapper
    def __dir__(self):
        keys = super(Sequential, self).__dir__()
        keys = [key for key in keys if not key.isdigit()]
        return keys

    @_copy_to_script_wrapper
    def __iter__(self) -> Iterator[Module]:
        return iter(self._modules.values())

    # NB: We can't really type check this function as the type of input
    # may change dynamically (as is tested in
    # TestScript.test_sequential_intermediary_types).  Cannot annotate
    # with Any as TorchScript expects a more precise type
    def forward(self, input):
        for module in self:
            input = module(input)
        return input


class ModuleList(Module):
    r"""Holds submodules in a list.

    :class:`~torch.nn.ModuleList` can be indexed like a regular Python list, but
    modules it contains are properly registered, and will be visible by all
    :class:`~torch.nn.Module` methods.

    Args:
        modules (iterable, optional): an iterable of modules to add

    Example::

        class MyModule(nn.Module):
            def __init__(self):
                super(MyModule, self).__init__()
                self.linears = nn.ModuleList([nn.Linear(10, 10) for i in range(10)])

            def forward(self, x):
                # ModuleList can act as an iterable, or be indexed using ints
                for i, l in enumerate(self.linears):
                    x = self.linears[i // 2](x) + l(x)
                return x
    """

    _modules: Dict[str, Module]  # type: ignore[assignment]

    def __init__(self, modules: Optional[Iterable[Module]] = None) -> None:
        super(ModuleList, self).__init__()
        if modules is not None:
            self += modules

    def _get_abs_string_index(self, idx):
        """Get the absolute index for the list of modules"""
        idx = operator.index(idx)
        if not (-len(self) <= idx < len(self)):
            raise IndexError('index {} is out of range'.format(idx))
        if idx < 0:
            idx += len(self)
        return str(idx)

    @_copy_to_script_wrapper
    def __getitem__(self, idx: int) -> Module:
        if isinstance(idx, slice):
            return self.__class__(list(self._modules.values())[idx])
        else:
            return self._modules[self._get_abs_string_index(idx)]

    def __setitem__(self, idx: int, module: Module) -> None:
        idx = self._get_abs_string_index(idx)
        return setattr(self, str(idx), module)

    def __delitem__(self, idx: Union[int, slice]) -> None:
        if isinstance(idx, slice):
            for k in range(len(self._modules))[idx]:
                delattr(self, str(k))
        else:
            delattr(self, self._get_abs_string_index(idx))
        # To preserve numbering, self._modules is being reconstructed with modules after deletion
        str_indices = [str(i) for i in range(len(self._modules))]
        self._modules = OrderedDict(list(zip(str_indices, self._modules.values())))

    @_copy_to_script_wrapper
    def __len__(self) -> int:
        return len(self._modules)

    @_copy_to_script_wrapper
    def __iter__(self) -> Iterator[Module]:
        return iter(self._modules.values())

    def __iadd__(self, modules: Iterable[Module]) -> 'ModuleList':
        return self.extend(modules)

    @_copy_to_script_wrapper
    def __dir__(self):
        keys = super(ModuleList, self).__dir__()
        keys = [key for key in keys if not key.isdigit()]
        return keys

    def insert(self, index: int, module: Module) -> None:
        r"""Insert a given module before a given index in the list.

        Args:
            index (int): index to insert.
            module (nn.Module): module to insert
        """
        for i in range(len(self._modules), index, -1):
            self._modules[str(i)] = self._modules[str(i - 1)]
        self._modules[str(index)] = module

    def append(self, module: Module) -> 'ModuleList':
        r"""Appends a given module to the end of the list.

        Args:
            module (nn.Module): module to append
        """
        self.add_module(str(len(self)), module)
        return self

    def extend(self, modules: Iterable[Module]) -> 'ModuleList':
        r"""Appends modules from a Python iterable to the end of the list.

        Args:
            modules (iterable): iterable of modules to append
        """
        if not isinstance(modules, container_abcs.Iterable):
            raise TypeError("ModuleList.extend should be called with an "
                            "iterable, but got " + type(modules).__name__)
        offset = len(self)
        for i, module in enumerate(modules):
            self.add_module(str(offset + i), module)
        return self

    # remove forward alltogether to fallback on Module's _forward_unimplemented


class ModuleDict(Module):
    r"""Holds submodules in a dictionary.

    :class:`~torch.nn.ModuleDict` can be indexed like a regular Python dictionary,
    but modules it contains are properly registered, and will be visible by all
    :class:`~torch.nn.Module` methods.

    :class:`~torch.nn.ModuleDict` is an **ordered** dictionary that respects

    * the order of insertion, and

    * in :meth:`~torch.nn.ModuleDict.update`, the order of the merged
      ``OrderedDict``, ``dict`` (started from Python 3.6) or another
      :class:`~torch.nn.ModuleDict` (the argument to
      :meth:`~torch.nn.ModuleDict.update`).

    Note that :meth:`~torch.nn.ModuleDict.update` with other unordered mapping
    types (e.g., Python's plain ``dict`` before Python version 3.6) does not
    preserve the order of the merged mapping.

    Args:
        modules (iterable, optional): a mapping (dictionary) of (string: module)
            or an iterable of key-value pairs of type (string, module)

    Example::

        class MyModule(nn.Module):
            def __init__(self):
                super(MyModule, self).__init__()
                self.choices = nn.ModuleDict({
                        'conv': nn.Conv2d(10, 10, 3),
                        'pool': nn.MaxPool2d(3)
                })
                self.activations = nn.ModuleDict([
                        ['lrelu', nn.LeakyReLU()],
                        ['prelu', nn.PReLU()]
                ])

            def forward(self, x, choice, act):
                x = self.choices[choice](x)
                x = self.activations[act](x)
                return x
    """

    _modules: Dict[str, Module]  # type: ignore[assignment]

    def __init__(self, modules: Optional[Mapping[str, Module]] = None) -> None:
        super(ModuleDict, self).__init__()
        if modules is not None:
            self.update(modules)

    @_copy_to_script_wrapper
    def __getitem__(self, key: str) -> Module:
        return self._modules[key]

    def __setitem__(self, key: str, module: Module) -> None:
        self.add_module(key, module)

    def __delitem__(self, key: str) -> None:
        del self._modules[key]

    @_copy_to_script_wrapper
    def __len__(self) -> int:
        return len(self._modules)

    @_copy_to_script_wrapper
    def __iter__(self) -> Iterator[str]:
        return iter(self._modules)

    @_copy_to_script_wrapper
    def __contains__(self, key: str) -> bool:
        return key in self._modules

    def clear(self) -> None:
        """Remove all items from the ModuleDict.
        """
        self._modules.clear()

    def pop(self, key: str) -> Module:
        r"""Remove key from the ModuleDict and return its module.

        Args:
            key (string): key to pop from the ModuleDict
        """
        v = self[key]
        del self[key]
        return v

    @_copy_to_script_wrapper
    def keys(self) -> Iterable[str]:
        r"""Return an iterable of the ModuleDict keys.
        """
        return self._modules.keys()

    @_copy_to_script_wrapper
    def items(self) -> Iterable[Tuple[str, Module]]:
        r"""Return an iterable of the ModuleDict key/value pairs.
        """
        return self._modules.items()

    @_copy_to_script_wrapper
    def values(self) -> Iterable[Module]:
        r"""Return an iterable of the ModuleDict values.
        """
        return self._modules.values()

    def update(self, modules: Mapping[str, Module]) -> None:
        r"""Update the :class:`~torch.nn.ModuleDict` with the key-value pairs from a
        mapping or an iterable, overwriting existing keys.

        .. note::
            If :attr:`modules` is an ``OrderedDict``, a :class:`~torch.nn.ModuleDict`, or
            an iterable of key-value pairs, the order of new elements in it is preserved.

        Args:
            modules (iterable): a mapping (dictionary) from string to :class:`~torch.nn.Module`,
                or an iterable of key-value pairs of type (string, :class:`~torch.nn.Module`)
        """
        if not isinstance(modules, container_abcs.Iterable):
            raise TypeError("ModuleDict.update should be called with an "
                            "iterable of key/value pairs, but got " +
                            type(modules).__name__)

        if isinstance(modules, (OrderedDict, ModuleDict, container_abcs.Mapping)):
            for key, module in modules.items():
                self[key] = module
        else:
            # modules here can be a list with two items
            for j, m in enumerate(modules):
                if not isinstance(m, container_abcs.Iterable):
                    raise TypeError("ModuleDict update sequence element "
                                    "#" + str(j) + " should be Iterable; is" +
                                    type(m).__name__)
                if not len(m) == 2:
                    raise ValueError("ModuleDict update sequence element "
                                     "#" + str(j) + " has length " + str(len(m)) +
                                     "; 2 is required")
                # modules can be Mapping (what it's typed at), or a list: [(name1, module1), (name2, module2)]
                # that's too cumbersome to type correctly with overloads, so we add an ignore here
                self[m[0]] = m[1]  # type: ignore[assignment]

    # remove forward alltogether to fallback on Module's _forward_unimplemented


class ParameterList(Module):
    r"""Holds parameters in a list.

    :class:`~torch.nn.ParameterList` can be indexed like a regular Python
    list, but parameters it contains are properly registered, and will be
    visible by all :class:`~torch.nn.Module` methods.

    Args:
        parameters (iterable, optional): an iterable of :class:`~torch.nn.Parameter` to add

    Example::

        class MyModule(nn.Module):
            def __init__(self):
                super(MyModule, self).__init__()
                self.params = nn.ParameterList([nn.Parameter(torch.randn(10, 10)) for i in range(10)])

            def forward(self, x):
                # ParameterList can act as an iterable, or be indexed using ints
                for i, p in enumerate(self.params):
                    x = self.params[i // 2].mm(x) + p.mm(x)
                return x
    """

    _parameters: Dict[str, 'Parameter']  # type: ignore[assignment]

    def __init__(self, parameters: Optional[Iterable['Parameter']] = None) -> None:
        super(ParameterList, self).__init__()
        self._initialized = True
        if parameters is not None:
            self += parameters

    def __setstate__(self, state):
        state['_initialized'] = False
        super(ParameterList, self).__setstate__(state)
        self._initialized = True

    def _get_abs_string_index(self, idx):
        """Get the absolute index for the list of modules"""
        idx = operator.index(idx)
        if not (-len(self) <= idx < len(self)):
            raise IndexError('index {} is out of range'.format(idx))
        if idx < 0:
            idx += len(self)
        return str(idx)

    @overload
    def __getitem__(self, idx: int) -> 'Parameter':
        ...

    @overload
    def __getitem__(self: T, idx: slice) -> T:
        ...

    def __getitem__(self, idx):
        if isinstance(idx, slice):
            return self.__class__(list(self._parameters.values())[idx])
        else:
            idx = self._get_abs_string_index(idx)
            return self._parameters[str(idx)]

    def __setitem__(self, idx: int, param: 'Parameter') -> None:
        idx = self._get_abs_string_index(idx)
        return self.register_parameter(str(idx), param)

    def __setattr__(self, key: Any, value: Any) -> None:
        if getattr(self, "_initialized", False):
            if not hasattr(self, key) and not isinstance(value, torch.nn.Parameter):
                warnings.warn("Setting attributes on ParameterList is not supported.")
        super(ParameterList, self).__setattr__(key, value)

    def __len__(self) -> int:
        return len(self._parameters)

    def __iter__(self) -> Iterator['Parameter']:
        return iter(self._parameters.values())

    def __iadd__(self, parameters: Iterable['Parameter']) -> 'ParameterList':
        return self.extend(parameters)

    def __dir__(self):
        keys = super(ParameterList, self).__dir__()
        keys = [key for key in keys if not key.isdigit()]
        return keys

    def append(self, parameter: 'Parameter') -> 'ParameterList':
        """Appends a given parameter at the end of the list.

        Args:
            parameter (nn.Parameter): parameter to append
        """
        self.register_parameter(str(len(self)), parameter)
        return self

    def extend(self, parameters: Iterable['Parameter']) -> 'ParameterList':
        """Appends parameters from a Python iterable to the end of the list.

        Args:
            parameters (iterable): iterable of parameters to append
        """
        if not isinstance(parameters, container_abcs.Iterable):
            raise TypeError("ParameterList.extend should be called with an "
                            "iterable, but got " + type(parameters).__name__)
        offset = len(self)
        for i, param in enumerate(parameters):
            self.register_parameter(str(offset + i), param)
        return self

    def extra_repr(self) -> str:
        child_lines = []
        for k, p in self._parameters.items():
            size_str = 'x'.join(str(size) for size in p.size())
            device_str = '' if not p.is_cuda else ' (GPU {})'.format(p.get_device())
            parastr = 'Parameter containing: [{} of size {}{}]'.format(
                torch.typename(p), size_str, device_str)
            child_lines.append('  (' + str(k) + '): ' + parastr)
        tmpstr = '\n'.join(child_lines)
        return tmpstr

    def __call__(self, input):
        raise RuntimeError('ParameterList should not be called.')

    def _replicate_for_data_parallel(self):
        warnings.warn("nn.ParameterList is being used with DataParallel but this is not "
                      "supported. This list will appear empty for the models replicated "
                      "on each GPU except the original one.")

        return super(ParameterList, self)._replicate_for_data_parallel()


class ParameterDict(Module):
    r"""Holds parameters in a dictionary.

    ParameterDict can be indexed like a regular Python dictionary, but parameters it
    contains are properly registered, and will be visible by all Module methods.

    :class:`~torch.nn.ParameterDict` is an **ordered** dictionary that respects

    * the order of insertion, and

    * in :meth:`~torch.nn.ParameterDict.update`, the order of the merged ``OrderedDict``
      or another :class:`~torch.nn.ParameterDict` (the argument to
      :meth:`~torch.nn.ParameterDict.update`).

    Note that :meth:`~torch.nn.ParameterDict.update` with other unordered mapping
    types (e.g., Python's plain ``dict``) does not preserve the order of the
    merged mapping.

    Args:
        parameters (iterable, optional): a mapping (dictionary) of
            (string : :class:`~torch.nn.Parameter`) or an iterable of key-value pairs
            of type (string, :class:`~torch.nn.Parameter`)

    Example::

        class MyModule(nn.Module):
            def __init__(self):
                super(MyModule, self).__init__()
                self.params = nn.ParameterDict({
                        'left': nn.Parameter(torch.randn(5, 10)),
                        'right': nn.Parameter(torch.randn(5, 10))
                })

            def forward(self, x, choice):
                x = self.params[choice].mm(x)
                return x
    """

    _parameters: Dict[str, 'Parameter']  # type: ignore[assignment]

    def __init__(self, parameters: Optional[Mapping[str, 'Parameter']] = None) -> None:
        super(ParameterDict, self).__init__()
        self._initialized = True
        if parameters is not None:
            self.update(parameters)

    def __setstate__(self, state):
        state['_initialized'] = False
        super(ParameterDict, self).__setstate__(state)
        self._initialized = True

    def __getitem__(self, key: str) -> 'Parameter':
        return self._parameters[key]

    def __setitem__(self, key: str, parameter: 'Parameter') -> None:
        self.register_parameter(key, parameter)

    def __delitem__(self, key: str) -> None:
        del self._parameters[key]

    def __setattr__(self, key: Any, value: Any) -> None:
        if getattr(self, "_initialized", False):
            if not hasattr(self, key) and not isinstance(value, torch.nn.Parameter):
                warnings.warn("Setting attributes on ParameterDict is not supported.")
        super(ParameterDict, self).__setattr__(key, value)

    def __len__(self) -> int:
        return len(self._parameters)

    def __iter__(self) -> Iterator[str]:
        return iter(self._parameters.keys())

    def __reversed__(self) -> Iterator[str]:
        return reversed(list(self._parameters.keys()))

    def copy(self) -> 'ParameterDict':
        """Returns a copy of this :class:`~torch.nn.ParameterDict` instance.
        """
        return ParameterDict(self._parameters.copy())

    def __contains__(self, key: str) -> bool:
        return key in self._parameters

    def setdefault(self, key: str, default: Optional['Parameter'] = None) -> 'Parameter':
        """If key is in the ParameterDict, return its parameter.
        If not, insert `key` with a parameter `default` and return `default`.
        `default` defaults to `None`.

        Args:
            key (string): key to set default for
            default (:class:`~torch.nn.Parameter`): the parameter set to the key
        """
        if key in self._parameters:
            return self._parameters[key]
        self[key] = default  # type: ignore[assignment]
        return self._parameters[key]

    def clear(self) -> None:
        """Remove all items from the ParameterDict.
        """
        self._parameters.clear()

    def pop(self, key: str) -> 'Parameter':
        r"""Remove key from the ParameterDict and return its parameter.

        Args:
            key (string): key to pop from the ParameterDict
        """
        v = self[key]
        del self[key]
        return v

    def popitem(self) -> Tuple[str, 'Parameter']:
        """Remove and return the last inserted `(key, parameter)` pair
        from the ParameterDict
        """
        return self._parameters.popitem()

    def get(self, key: str, default: Optional['Parameter'] = None) -> 'Parameter | None':
        r"""Return the parameter associated with key if present.
        Otherwise return default if provided, None if not.

        Args:
            key (string): key to get from the ParameterDict
            default (Parameter, optional): value to return if key not present
        """
        return self._parameters.get(key, default)

    def fromkeys(self, keys: Iterable['str'], default: Optional['Parameter'] = None) -> 'ParameterDict':
        r"""Return a new ParameterDict with the keys provided

        Args:
            keys (iterable, string): keys to make the new ParameterDict from
            default (Parameter, optional): value to set for all keys
        """
        return ParameterDict(self._parameters.fromkeys(keys, default))  # type: ignore[arg-type]

    def keys(self) -> Iterable[str]:
        r"""Return an iterable of the ParameterDict keys.
        """
        return self._parameters.keys()

    def items(self) -> Iterable[Tuple[str, 'Parameter']]:
        r"""Return an iterable of the ParameterDict key/value pairs.
        """
        return self._parameters.items()

    def values(self) -> Iterable['Parameter']:
        r"""Return an iterable of the ParameterDict values.
        """
        return self._parameters.values()

    def update(self, parameters: Mapping[str, 'Parameter']) -> None:
        r"""Update the :class:`~torch.nn.ParameterDict` with the key-value pairs from a
        mapping or an iterable, overwriting existing keys.

        .. note::
            If :attr:`parameters` is an ``OrderedDict``, a :class:`~torch.nn.ParameterDict`, or
            an iterable of key-value pairs, the order of new elements in it is preserved.

        Args:
            parameters (iterable): a mapping (dictionary) from string to
                :class:`~torch.nn.Parameter`, or an iterable of
                key-value pairs of type (string, :class:`~torch.nn.Parameter`)
        """
        if not isinstance(parameters, container_abcs.Iterable):
            raise TypeError("ParametersDict.update should be called with an "
                            "iterable of key/value pairs, but got " +
                            type(parameters).__name__)

        if isinstance(parameters, (OrderedDict, ParameterDict)):
            for key, parameter in parameters.items():
                self[key] = parameter
        elif isinstance(parameters, container_abcs.Mapping):
            for key, parameter in sorted(parameters.items()):
                self[key] = parameter
        else:
            for j, p in enumerate(parameters):
                if not isinstance(p, container_abcs.Iterable):
                    raise TypeError("ParameterDict update sequence element "
                                    "#" + str(j) + " should be Iterable; is" +
                                    type(p).__name__)
                if not len(p) == 2:
                    raise ValueError("ParameterDict update sequence element "
                                     "#" + str(j) + " has length " + str(len(p)) +
                                     "; 2 is required")
                # parameters as length-2 list too cumbersome to type, see ModuleDict.update comment
                self[p[0]] = p[1]  # type: ignore[assignment]

    def extra_repr(self) -> str:
        child_lines = []
        for k, p in self._parameters.items():
            size_str = 'x'.join(str(size) for size in p.size())
            device_str = '' if not p.is_cuda else ' (GPU {})'.format(p.get_device())
            parastr = 'Parameter containing: [{} of size {}{}]'.format(
                torch.typename(p), size_str, device_str)
            child_lines.append('  (' + k + '): ' + parastr)
        tmpstr = '\n'.join(child_lines)
        return tmpstr

    def __call__(self, input):
        raise RuntimeError('ParameterDict should not be called.')

    def _replicate_for_data_parallel(self):
        warnings.warn("nn.ParameterDict is being used with DataParallel but this is not "
                      "supported. This dict will appear empty for the models replicated "
                      "on each GPU except the original one.")

        return super(ParameterDict, self)._replicate_for_data_parallel()

    def __or__(self, other: 'ParameterDict') -> 'ParameterDict':
        copy = self.copy()
        copy.update(other._parameters)
        return copy

    def __ror__(self, other: 'ParameterDict') -> 'ParameterDict':
        copy = other.copy()
        copy.update(self._parameters)
        return copy

    def __ior__(self, other : 'ParameterDict') -> 'ParameterDict':
        self.update(other._parameters)
        return self
