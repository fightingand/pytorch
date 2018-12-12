'''Functionality for Python <-> C++ frontend inter-op.'''

from torch import nn

class OrderedDictWrapper(object):
    '''
    A wrapper around a C++ OrderedDict that dynamically evaluates the
    OrderedDict getter on a bound C++ module, such that new changes on the C++
    side are picked up. Otherwise accessing e.g. ``cpp_module._parameters`` just
    once would get a frozen copy of the parameters at the time of access.
    ``torch.nn.Module`` accesses ``_parameters`` et al. via ``self.__dict__`` so
    using properties does not work.
    '''
    def __init__(self, cpp_dict_getter):
        self.cpp_dict_getter = cpp_dict_getter

    @property
    def cpp_dict(self):
        return self.cpp_dict_getter()

    # Magic methods cannot be assigned dynamically and bypass ``getattr``, so we
    # must manually override them.

    def __iter__(self):
        return self.cpp_dict.__iter__()

    def __len__(self):
        return self.cpp_dict.__len__()

    def __contains__(self, key):
        return self.cpp_dict.__contains__(key)

    def __getitem__(self, key):
        return self.cpp_dict.__getitem__(key)

    def __getattr__(self, name):
        return getattr(self.cpp_dict, name)


class ModuleWrapper(nn.Module):
    '''
    A subclass of ``torch.nn.Module`` that wraps a C++ frontend module and
    delegates all access.
    '''
    def __init__(self, cpp_module):
        # Assign before the super class constructor so ``self.training`` can be
        # assigned to in the super class constructor.
        self.cpp_module = cpp_module
        super(ModuleWrapper, self).__init__()
        self._parameters = OrderedDictWrapper(lambda: cpp_module._parameters)
        self._buffers = OrderedDictWrapper(lambda: cpp_module._buffers)
        self._modules = OrderedDictWrapper(lambda: cpp_module._modules)
        for attr in dir(cpp_module):
            # Skip magic methods and the three attributes above.
            if not attr.startswith("_"):
                setattr(self, attr, getattr(self.cpp_module, attr))

    @property
    def training(self):
        return self.cpp_module.training

    @training.setter
    def training(self, mode):
        self.cpp_module.train(mode)

    def __repr__(self):
        return self.cpp_module.__repr__()
