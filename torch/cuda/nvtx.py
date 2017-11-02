import os
import glob
import ctypes
import platform

lib = None

__all__ = ['range_push', 'range_pop', 'mark']

WINDOWS_HOME = 'C:/Program Files/NVIDIA Corporation/NvToolsExt'

def find_nvToolsExt_windows_lib():
    NVTOOLEXT_HOME = os.getenv('NVTOOLSEXT_PATH', WINDOWS_HOME)
    if os.path.exists(NVTOOLEXT_HOME):
        lib_paths = glob.glob(NVTOOLEXT_HOME + '/bin/x64/nvToolsExt*.dll')
        if len(lib_paths) > 0:
            lib_path = nvToolsExt_lib_paths[0]
            lib_name = os.path.basename(nvToolsExt_lib_path)
            lib = os.path.splitext(nvToolsExt_lib_name)[0]
            lib_path = os.path.dirname(nvToolsExt_lib_path).replace('\\', '/')

            os.environ['PATH'] = nvToolsExt_lib_path + ';' + os.environ['PATH']

            return ctypes.cdll.LoadLibrary(nvToolsExt_lib)
    return None


def _libnvToolsExt():
    global lib
    if lib is None:
        if platform.system() != 'Windows':
            lib = ctypes.cdll.LoadLibrary(None)
        else:
            lib = find_nvToolsExt_windows_lib()
        lib.nvtxMarkA.restype = None
    return lib


def range_push(msg):
    """
    Pushes a range onto a stack of nested range span.  Returns zero-based
    depth of the range that is started.

    Arguments:
        msg (string): ASCII message to associate with range
    """
    if _libnvToolsExt() is None:
        raise RuntimeError('Unable to load nvToolsExt library')
    return lib.nvtxRangePushA(ctypes.c_char_p(msg.encode("ascii")))


def range_pop():
    """
    Pops a range off of a stack of nested range spans.  Returns the
    zero-based depth of the range that is ended.
    """
    if _libnvToolsExt() is None:
        raise RuntimeError('Unable to load nvToolsExt library')
    return lib.nvtxRangePop()


def mark(msg):
    """
    Describe an instantaneous event that occurred at some point.

    Arguments:
        msg (string): ASCII message to associate with the event.
    """
    if _libnvToolsExt() is None:
        raise RuntimeError('Unable to load nvToolsExt library')
    return lib.nvtxMarkA(ctypes.c_char_p(msg.encode("ascii")))
