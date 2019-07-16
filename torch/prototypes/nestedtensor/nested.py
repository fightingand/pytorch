import torch


def is_nested_tensor(obj):
    return isinstance(obj, NestedTensor)


# Arguments match torch.tensor
def make_nested_tensor(data, dtype=None, device=None, requires_grad=False, pin_memory=False):
    if is_nested_tensor(data):
        # This is consistent with torch.tensor(torch.Tensor)
        return data.clone().detach()
    elif torch.is_tensor(data):
        # The user has the right to expect a NestedTensor from this
        # function, but we can't meaningfully provide one if passed a Tensor
        raise ValueError("Can't construct a NestedTensor from a Tensor")
    else:
        assert isinstance(data, list)
        if len(data) == 0:
            return NestedTensor([])
        for data_ in data:
            assert(torch.is_tensor(data_))
        tensors = []
        for data_ in data:
            # torch.tensor copies on construction
            new_data = data_.clone().detach()
            new_data = new_data.to(dtype)
            new_data = new_data.to(device)
            new_data = new_data.requires_grad_(requires_grad)
            new_data = new_data.pin_memory()
            tensors.append(new_data)

        return NestedTensor(tensors)


class NestedTensor():
    # The attributes must match across all constiuents
    # and default to the empty Tensor's if the given list
    # is empty.
    #
    # The NestedTensor's attributes then become that of its
    # constiuents.
    #
    # Attributes:
    #     dim
    #     layout
    #     device
    #     dtype
    #     requires_grad
    #     is_pinned
    def __init__(self, tensors):
        for tensor in tensors:
            assert torch.is_tensor(tensor)
        self.tensors = tensors
        if len(tensors):
            self.dim = tensors[0].dim()
            self.layout = tensors[0].layout
            self.device = tensors[0].device
            self.dtype = tensors[0].dtype
            requires_grad = tensors[0].requires_grad
            is_pinned = tensors[0].is_pinned()
            for tensor in tensors:
                if not (self.dim == tensor.dim() and
                        self.layout == tensor.layout and
                        self.device == tensor.device and
                        self.dtype == tensor.dtype and
                        requires_grad == tensor.requires_grad and
                        is_pinned == tensor.is_pinned()):
                    raise ValueError("Each passed Tensor "
                            "must match in dim, layout, "
                            "device, dtype and requires_grad")
        else:
            # Carrying around information as member variables vs.
            # checking one entry of the owned Tensors is annoying
            # and error-prone. Carrying around an is_empty attribute
            # to hide the fact that we carry around a list with a
            # single empty Tensor is also annoying and error-prone.
            # Both are not worth it for a minor feature.
            raise ValueError("We do not support empty lists for now.")

    def __getattribute__(self, attr):
        if attr == 'shape':
            raise NotImplementedError()
        if attr == 'requires_grad':
            return self.tensors[0].requires_grad
        return super().__getattribute__(attr)



    def __len__(self):
        return len(self.tensors)

    def __bool__(self):
        # Explicitly punt on this until we have fully
        # specificed reduction semantics.
        raise NotImplementedError()

    def __str__(self):
        tensors = self.unbind()
        result = "nestedtensor([\n"
        for tensor in tensors:
            result += "  " + tensor.__str__() + "\n"
        result += "])"
        return result

    def __repr__(self):
        tensors = self.unbind()
        result = "nestedtensor([\n"
        for tensor in tensors:
            result += "  " + tensor.__repr__() + "\n"
        result += "])"
        return result

    def is_empty(self):
        return len(self.tensors) == 0

    def __loop__apply(self, fn):
        tensors = []
        for tensor in self.tensors:
            tensors.append(fn(tensor))
        return NestedTensor(tensors)

    def nested_size(self):
        tensors = self.unbind()
        sizes = []
        for tensor in tensors:
            sizes.append(tensor.size())
        return tuple(sizes)

    # TODO: Not covered by RFC! NestedTensor 0.0.2 will talk about reductions.
    def all(self):
        ret = True
        for tensor in self.tensors:
            ret = ret and tensor.all()
        return ret

    # TODO: Not covered by RFC! NestedTensor 0.0.2 will talk about reductions.
    def any(self):
        ret = False
        for tensor in self.tensors:
            ret = ret or tensor.any()
        return ret

    # Tensor ops
    def detach(self):
        return self.__loop__apply(lambda x: x.detach())

    def backward(self):
        for tensor in self.tensors:
            tensor.backward()

    def clone(self):
        return self.__loop__apply(lambda x: x.clone())

    def type(self, dtype):
        return self.__loop__apply(lambda x: x.type(dtype))

    def to(self, device):
        return self.__loop__apply(lambda x: x.to(device))

    def requires_grad_(self, requires_grad=True):
        return self.__loop__apply(lambda x: x.requires_grad_(x, requires_grad_=requires_grad))

    def unbind(self):
        return tuple(self.tensors)
