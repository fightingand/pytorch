import unittest
import torch
from torch import ops
import torch.jit as jit

# from model import Model, get_custom_op_library_path


def test_equality(f, cmp_key):
    obj1 = f()
    obj2 = jit.script(f)()
    return (cmp_key(obj1), cmp_key(obj2))

class TestCustomOperators(unittest.TestCase):
    def setUp(self):
        ops.load_library("build/custom_class.cpython-37m-darwin.so")

    def test_no_return_class(self):
        def f():
            val = torch.classes.Foo(5, 3)
            return val.info()
        self.assertEqual(*test_equality(f, lambda x: x))

    def test_constructor_with_args(self):
        def f():
            val = torch.classes.Foo(5, 3)
            return val
        self.assertEqual(*test_equality(f, lambda x: x.info()))

    def test_function_call_with_args(self):
        def f():
            val = torch.classes.Foo(5, 3)
            val.increment(1)
            return val

        self.assertEqual(*test_equality(f, lambda x: x.info()))

    def test_function_method_wrong_type(self):
        def f():
            val = torch.classes.Foo(5, 3)
            val.increment("asdf")
            return val

        with self.assertRaisesRegex(RuntimeError, "Expected"):
            jit.script(f)()

    def test_input_class_type(self):
        def f():
            val = torch.classes.Foo(1, 2)
            val2 = torch.classes.Foo(2, 3)
            val.combine(val2)
            return val

        print(jit.script(f)())
        # self.assertEqual(*test_equality(f, lambda x: x))


if __name__ == "__main__":
    unittest.main()
