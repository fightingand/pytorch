#!/usr/bin/env bash

# Shell script used to build the aten/*, caffe2/*, and third_party/*
# dependencies prior to linking libraries and passing headers to the Python
# extension compilation stage. This file is used from setup.py, but can also be
# called standalone to compile the libraries outside of the overall PyTorch
# build process.
#
# TODO: Replace this with the root-level CMakeLists.txt
#
# To build on Android:
# ANDROID_NDK=~/Android/Sdk/ndk-bundle/ BUILD_TORCH=ON ONNX_NAMESPACE=onnx_torch PYTORCH_PYTHON=/usr/bin/python3 PROTOBUF_PROTOC_EXECUTABLE=../build_host_protoc/bin/protoc ANDROID_ABI=x86 ../tools/build_pytorch_libs.sh caffe2

set -e
if [[ $VERBOSE_SCRIPT == '1' ]]; then
  set -x
  report() {
    echo "$@"
  }
else
  report() {
    :
  }
fi

SYNC_COMMAND="cp"
if [ -x "$(command -v rsync)" ]; then
    SYNC_COMMAND="rsync -lptgoD"
fi

# We test the presence of cmake3 (for platforms like CentOS and Ubuntu 14.04)
# and use the newer of cmake and cmake3 if so.
CMAKE_COMMAND="cmake"
if [[ -x "$(command -v cmake3)" ]]; then
    if [[ -x "$(command -v cmake)" ]]; then
        # have both cmake and cmake3, compare versions
        # Usually cmake --version returns two lines,
        #   cmake version #.##.##
        #   <an empty line>
        # On the nightly machines it returns one line
        #   cmake3 version 3.11.0 CMake suite maintained and supported by Kitware (kitware.com/cmake).
        # Thus we extract the line that has 'version' in it and hope the actual
        # version number is gonna be the 3rd element
        CMAKE_VERSION=$(cmake --version | grep 'version' | awk '{print $3}' | awk -F. '{print $1"."$2"."$3}')
        CMAKE3_VERSION=$(cmake3 --version | grep 'version' | awk '{print $3}' | awk -F. '{print $1"."$2"."$3}')
        CMAKE3_NEEDED=$($PYTORCH_PYTHON -c "from distutils.version import StrictVersion; print(1 if StrictVersion(\"${CMAKE_VERSION}\") < StrictVersion(\"3.5.0\") and StrictVersion(\"${CMAKE3_VERSION}\") > StrictVersion(\"${CMAKE_VERSION}\") else 0)")
    else
        # don't have cmake
        CMAKE3_NEEDED=1
    fi
    if [[ $CMAKE3_NEEDED == "1" ]]; then
        CMAKE_COMMAND="cmake3"
    fi
    unset CMAKE_VERSION CMAKE3_VERSION CMAKE3_NEEDED
fi

# Options for building only a subset of the libraries
USE_CUDA=0
USE_FBGEMM=0
USE_ROCM=0
USE_NNPACK=0
USE_MKLDNN=0
USE_QNNPACK=0
USE_GLOO_IBVERBS=0
CAFFE2_STATIC_LINK_CUDA=0
RERUN_CMAKE=1
while [[ $# -gt 0 ]]; do
    case "$1" in
      --dont-rerun-cmake)
          RERUN_CMAKE=0
          ;;
      --use-cuda)
          USE_CUDA=1
          ;;
      --use-distributed)
          USE_DISTRIBUTED=1
          ;;
      --use-fbgemm)
          USE_FBGEMM=1
          ;;
      --use-rocm)
          USE_ROCM=1
          ;;
      --use-nnpack)
          USE_NNPACK=1
          ;;
      --use-mkldnn)
          USE_MKLDNN=1
          ;;
      --use-qnnpack)
          USE_QNNPACK=1
          ;;
      --use-gloo-ibverbs)
          USE_GLOO_IBVERBS=1
          ;;
      --cuda-static-link)
          CAFFE2_STATIC_LINK_CUDA=1
          ;;
      *)
          break
          ;;
    esac
    shift
done

CMAKE_INSTALL=${CMAKE_INSTALL-make install}

BUILD_SHARED_LIBS=${BUILD_SHARED_LIBS-ON}

# Save user specified env vars, we will manually propagate them
# to cmake.  We copy distutils semantics, referring to
# cpython/Lib/distutils/sysconfig.py as the source of truth
USER_CFLAGS=""
USER_LDFLAGS=""
if [[ -n "$LDFLAGS" ]]; then
  USER_LDFLAGS="$USER_LDFLAGS $LDFLAGS"
fi
if [[ -n "$CFLAGS" ]]; then
  USER_CFLAGS="$USER_CFLAGS $CFLAGS"
  USER_LDFLAGS="$USER_LDFLAGS $CFLAGS"
fi
if [[ -n "$CPPFLAGS" ]]; then
  # Unlike distutils, NOT modifying CXX
  USER_C_CFLAGS="$USER_CFLAGS $CPPFLAGS"
  USER_LDFLAGS="$USER_LDFLAGS $CPPFLAGS"
fi

# Use ccache if available (this path is where Homebrew installs ccache symlinks)
if [ "$(uname)" == 'Darwin' ]; then
  if [ -d '/usr/local/opt/ccache/libexec' ]; then
    CCACHE_WRAPPER_PATH=/usr/local/opt/ccache/libexec
  fi
fi

BASE_DIR=$(cd $(dirname "$0")/.. && printf "%q\n" "$(pwd)")
TORCH_LIB_DIR="$BASE_DIR/torch/lib"
INSTALL_DIR="$TORCH_LIB_DIR/tmp_install"
THIRD_PARTY_DIR="$BASE_DIR/third_party"

C_FLAGS=""
# Workaround OpenMPI build failure
# ImportError: /build/pytorch-0.2.0/.pybuild/pythonX.Y_3.6/build/torch/_C.cpython-36m-x86_64-linux-gnu.so: undefined symbol: _ZN3MPI8Datatype4FreeEv
# https://bugs.debian.org/cgi-bin/bugreport.cgi?bug=686926
C_FLAGS="${C_FLAGS} -DOMPI_SKIP_MPICXX=1"
LDFLAGS=""
LD_POSTFIX=".so"
if [[ $(uname) == 'Darwin' ]]; then
    LDFLAGS="$LDFLAGS -Wl,-rpath,@loader_path"
    LD_POSTFIX=".dylib"
else
    if [[ $USE_ROCM -eq 1 ]]; then
        LDFLAGS="$LDFLAGS -Wl,-rpath,\\\\\\\$ORIGIN"
    else
        LDFLAGS="$LDFLAGS -Wl,-rpath,\$ORIGIN"
    fi
fi
CPP_FLAGS=" -std=c++11 "
THD_FLAGS=""
# Gloo infiniband support
if [[ $USE_GLOO_IBVERBS -eq 1 ]]; then
    GLOO_FLAGS+=" -DUSE_IBVERBS=1"
    THD_FLAGS="-DUSE_GLOO_IBVERBS=1"
fi
CUDA_NVCC_FLAGS=$C_FLAGS
if [[ -z "$CUDA_DEVICE_DEBUG" ]]; then
  CUDA_DEVICE_DEBUG=0
fi
if [ -z "$MAX_JOBS" ]; then
  MAX_JOBS="$(getconf _NPROCESSORS_ONLN)"
fi

BUILD_TYPE="Release"
if [[ -n "$DEBUG" && $DEBUG -ne 0 ]]; then
  BUILD_TYPE="Debug"
elif [[ -n "$REL_WITH_DEB_INFO" && $REL_WITH_DEB_INFO -ne 0 ]]; then
  BUILD_TYPE="RelWithDebInfo"
fi

report "Building in $BUILD_TYPE mode"

function path_remove {
  # Delete path by parts so we can never accidentally remove sub paths
  PATH=${PATH//":$1:"/":"} # delete any instances in the middle
  PATH=${PATH/#"$1:"/} # delete any instance at the beginning
  PATH=${PATH/%":$1"/} # delete any instance in the at the end
}

# purposefully not using build() because we need Caffe2 to build the same
# regardless of whether it is inside PyTorch or not, so it
# cannot take any special flags
# special flags need to be part of the Caffe2 build itself
#
# However, we do explicitly pass library paths when setup.py has already
# detected them (to ensure that we have a consistent view between the
# PyTorch and Caffe2 builds.)
function build_caffe2() {
  # pwd is pytorch_root/build

  if [[ -n $ANDROID_ABI ]]; then
    EXTRA_CAFFE2_CMAKE_FLAGS+=("-DCAFFE2_CUSTOM_PROTOC_EXECUTABLE=$(pwd)/../build_host_protoc/bin/protoc")
    EXTRA_CAFFE2_CMAKE_FLAGS+=("-DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK/build/cmake/android.toolchain.cmake")
    EXTRA_CAFFE2_CMAKE_FLAGS+=("-DANDROID_TOOLCHAIN=clang")
    EXTRA_CAFFE2_CMAKE_FLAGS+=("-DUSE_OPENMP=OFF")
    EXTRA_CAFFE2_CMAKE_FLAGS+=("-DANDROID_NDK=$ANDROID_NDK")
    EXTRA_CAFFE2_CMAKE_FLAGS+=("-DANDROID_ABI=$ANDROID_ABI")
    EXTRA_CAFFE2_CMAKE_FLAGS+=("-DANDROID_NATIVE_API_LEVEL=21")
    EXTRA_CAFFE2_CMAKE_FLAGS+=("-DANDROID_CPP_FEATURES=rtti exceptions")
    EXTRA_CAFFE2_CMAKE_FLAGS+=("-DUSE_MOBILE_OPENGL=OFF")
    EXTRA_CAFFE2_CMAKE_FLAGS+=("-DUSE_MKL=OFF")
    EXTRA_CAFFE2_CMAKE_FLAGS+=("-Dprotobuf_BUILD_PROTOC_BINARIES=OFF")
    BUILD_PYTHON=OFF
    BUILD_SHARED_LIBS=OFF
    BUILD_BINARY=OFF
    BUILD_TEST=OFF
    USE_NNPACK=ON
    USE_DISTRIBUTED=OFF
    BLAS=Eigen
    BUILD_CAFFE2_OPS=OFF
    export CMAKE_MAKE_PROGRAM=ninja
  fi

  # TODO change these to CMAKE_ARGS for consistency
  if [[ -z $EXTRA_CAFFE2_CMAKE_FLAGS ]]; then
    EXTRA_CAFFE2_CMAKE_FLAGS=()
  fi
  if [[ -n $CCACHE_WRAPPER_PATH ]]; then
    EXTRA_CAFFE2_CMAKE_FLAGS+=("-DCMAKE_C_COMPILER=$CCACHE_WRAPPER_PATH/gcc")
    EXTRA_CAFFE2_CMAKE_FLAGS+=("-DCMAKE_CXX_COMPILER=$CCACHE_WRAPPER_PATH/g++")
  fi
  if [[ -n $CMAKE_PREFIX_PATH ]]; then
    EXTRA_CAFFE2_CMAKE_FLAGS+=("-DCMAKE_PREFIX_PATH=$CMAKE_PREFIX_PATH")
  fi
  if [[ -n $BLAS ]]; then
    EXTRA_CAFFE2_CMAKE_FLAGS+=("-DBLAS=$BLAS")
  fi
  if [[ -n $CUDA_NVCC_EXECUTABLE ]]; then
    EXTRA_CAFFE2_CMAKE_FLAGS+=("-DCUDA_NVCC_EXECUTABLE=$CUDA_NVCC_EXECUTABLE")
  fi
  if [[ -n $USE_REDIS ]]; then
    EXTRA_CAFFE2_CMAKE_FLAGS+=("-DUSE_REDIS=$USE_REDIS")
  fi
  if [[ -n $USE_GLOG ]]; then
    EXTRA_CAFFE2_CMAKE_FLAGS+=("-DUSE_GLOG=$USE_GLOG")
  fi
  if [[ -n $USE_GFLAGS ]]; then
    EXTRA_CAFFE2_CMAKE_FLAGS+=("-DUSE_GFLAGS=$USE_GFLAGS")
  fi

  if [[ $RERUN_CMAKE -eq 1 ]] || [ ! -f CMakeCache.txt ]; then
      ${CMAKE_COMMAND} $BASE_DIR \
		       ${CMAKE_GENERATOR} \
		       -DPYTHON_EXECUTABLE=$PYTORCH_PYTHON \
		       -DPYTHON_LIBRARY="${PYTORCH_PYTHON_LIBRARY}" \
		       -DPYTHON_INCLUDE_DIR="${PYTORCH_PYTHON_INCLUDE_DIR}" \
		       -DBUILDING_WITH_TORCH_LIBS=ON \
		       -DTORCH_BUILD_VERSION="$PYTORCH_BUILD_VERSION" \
		       -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
		       -DBUILD_TORCH=$BUILD_TORCH \
		       -DBUILD_PYTHON=$ \
		       -DBUILD_SHARED_LIBS=$BUILD_SHARED_LIBS \
		       -DBUILD_BINARY=$BUILD_BINARY \
		       -DBUILD_TEST=$BUILD_TEST \
		       -DINSTALL_TEST=$INSTALL_TEST \
		       -DBUILD_CAFFE2_OPS=$BUILD_CAFFE2_OPS \
		       -DONNX_NAMESPACE=$ONNX_NAMESPACE \
		       -DUSE_CUDA=$USE_CUDA \
		       -DUSE_DISTRIBUTED=$USE_DISTRIBUTED \
		       -DUSE_FBGEMM=$USE_FBGEMM \
		       -DUSE_NUMPY=$USE_NUMPY \
		       -DNUMPY_INCLUDE_DIR=$NUMPY_INCLUDE_DIR \
		       -DUSE_SYSTEM_NCCL=$USE_SYSTEM_NCCL \
		       -DNCCL_INCLUDE_DIR=$NCCL_INCLUDE_DIR \
		       -DNCCL_ROOT_DIR=$NCCL_ROOT_DIR \
		       -DNCCL_SYSTEM_LIB=$NCCL_SYSTEM_LIB \
		       -DCAFFE2_STATIC_LINK_CUDA=$CAFFE2_STATIC_LINK_CUDA \
		       -DUSE_ROCM=$USE_ROCM \
		       -DUSE_NNPACK=$USE_NNPACK \
		       -DUSE_LEVELDB=$USE_LEVELDB \
		       -DUSE_LMDB=$USE_LMDB \
		       -DUSE_OPENCV=$USE_OPENCV \
		       -DUSE_QNNPACK=$USE_QNNPACK \
		       -DUSE_TENSORRT=$USE_TENSORRT \
		       -DUSE_FFMPEG=$USE_FFMPEG \
		       -DUSE_SYSTEM_EIGEN_INSTALL=OFF \
		       -DCUDNN_INCLUDE_DIR=$CUDNN_INCLUDE_DIR \
		       -DCUDNN_LIB_DIR=$CUDNN_LIB_DIR \
		       -DCUDNN_LIBRARY=$CUDNN_LIBRARY \
		       -DUSE_MKLDNN=$USE_MKLDNN \
		       -DNCCL_EXTERNAL=$USE_CUDA \
		       -DCMAKE_INSTALL_PREFIX="$INSTALL_DIR" \
		       -DCMAKE_C_FLAGS="$USER_CFLAGS" \
		       -DCMAKE_CXX_FLAGS="$USER_CFLAGS" \
		       -DCMAKE_EXE_LINKER_FLAGS="$LDFLAGS $USER_LDFLAGS" \
		       -DCMAKE_SHARED_LINKER_FLAGS="$LDFLAGS $USER_LDFLAGS" \
		       $GLOO_FLAGS \
		       -DTHD_SO_VERSION=1 \
		       $THD_FLAGS \
		       "${EXTRA_CAFFE2_CMAKE_FLAGS[@]}"
      # STOP!!! Are you trying to add a C or CXX flag?  Add it
      # to CMakeLists.txt and aten/CMakeLists.txt, not here.
      # We need the vanilla cmake build to work.
  fi

  ${CMAKE_INSTALL} -j"$MAX_JOBS"
  if ls build.ninja 2>&1 >/dev/null; then
      # in cmake, .cu compilation involves generating certain intermediates
      # such as .cu.o and .cu.depend, and these intermediates finally get compiled
      # into the final .so.
      # Ninja updates build.ninja's timestamp after all dependent files have been built,
      # and re-kicks cmake on incremental builds if any of the dependent files
      # have a timestamp newer than build.ninja's timestamp.
      # There is a cmake bug with the Ninja backend, where the .cu.depend files
      # are still compiling by the time the build.ninja timestamp is updated,
      # so the .cu.depend file's newer timestamp is screwing with ninja's incremental
      # build detector.
      # This line works around that bug by manually updating the build.ninja timestamp
      # after the entire build is finished.
      touch build.ninja
  fi

  # Install Python proto files
  if [[ "$BUILD_PYTHON" == 'ON' ]]; then

      if [[ $VERBOSE_SCRIPT == '1' ]]; then
        report "Copying Caffe2 proto files from $(pwd)/caffe2/proto to  $(cd .. && pwd)/caffe2/proto"
        report "All the files in caffe2/proto are $(find caffe2/proto)"
      fi

      for proto_file in $(pwd)/caffe2/proto/*.py; do
          # __init__.py is not auto-generated, copying it breaks
          # mod-times for rebuild logic
          if [[ "$proto_file" != "$(pwd)/caffe2/proto/__init__.py" ]]; then
            cp $proto_file "$(pwd)/../caffe2/proto/"
          fi
      done
  fi
}

# In the torch/lib directory, create an installation directory
mkdir -p $INSTALL_DIR

# Build
for arg in "$@"; do
    if [[ "$arg" == "caffe2" ]]; then
        build_caffe2
    else
        pushd "$THIRD_PARTY_DIR"
        build $arg
        popd
    fi
done

pushd $TORCH_LIB_DIR > /dev/null

# If all the builds succeed we copy the libraries, headers,
# binaries to torch/lib
report "tools/build_pytorch_libs.sh succeeded at $(date)"
report "removing $INSTALL_DIR/lib/cmake and $INSTALL_DIR/lib/python"
rm -rf "$INSTALL_DIR/lib/cmake"
rm -rf "$INSTALL_DIR/lib/python"

report "Copying $INSTALL_DIR/lib to $(pwd)"
$SYNC_COMMAND -r "$INSTALL_DIR/lib"/* .
if [ -d "$INSTALL_DIR/lib64/" ]; then
    $SYNC_COMMAND -r "$INSTALL_DIR/lib64"/* .
fi
report "Copying $(cd ../.. && pwd)/aten/src/generic/THNN.h to $(pwd)"
$SYNC_COMMAND ../../aten/src/THNN/generic/THNN.h .
$SYNC_COMMAND ../../aten/src/THCUNN/generic/THCUNN.h .

report "Copying $INSTALL_DIR/include to $(pwd)"
$SYNC_COMMAND -r "$INSTALL_DIR/include" .
if [ -d "$INSTALL_DIR/bin/" ]; then
    $SYNC_COMMAND -r "$INSTALL_DIR/bin/"/* .
fi

# Copy the test files to pytorch/caffe2 manually
# They were built in pytorch/torch/lib/tmp_install/test
# Why do we do this? So, setup.py has this section called 'package_data' which
# you need to specify to include non-default files (usually .py files).
# package_data takes a map from 'python package' to 'globs of files to
# include'. By 'python package', it means a folder with an __init__.py file
# that's not excluded in the find_packages call earlier in setup.py. So to
# include our cpp_test into the site-packages folder in
# site-packages/caffe2/cpp_test, we have to copy the cpp_test folder into the
# root caffe2 folder and then tell setup.py to include them. Having another
# folder like site-packages/caffe2_cpp_test would also be possible by adding a
# caffe2_cpp_test folder to pytorch with an __init__.py in it.
if [[ "$INSTALL_TEST" == "ON" ]]; then
    echo "Copying $INSTALL_DIR/test to $BASE_DIR/caffe2/cpp_test"
    mkdir -p "$BASE_DIR/caffe2/cpp_test/"
    $SYNC_COMMAND -r "$INSTALL_DIR/test/"/* "$BASE_DIR/caffe2/cpp_test/"
fi

popd > /dev/null
