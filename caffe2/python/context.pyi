from typing import Optional, Type, TypeVar

T = TypeVar('T')

class Managed:
    @classmethod
    def current(cls: Type[T], value: Optional[T] = None, required: bool = True) -> T: ...

    def __call__(self, func: T) -> T: ...

    def __enter__(self: T) -> T: ...

class DefaultManaged(Managed): ...
