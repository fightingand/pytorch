#!/usr/bin/env python3

from __future__ import print_function
import collections
import os
import sys
import logging

BANNER = "Auto-generated by generate-wrappers.py script. Do not modify"
WRAPPER_SRC_NAMES = {
    "PROD_SCALAR_MICROKERNEL_SRCS": None,
    "PROD_FMA_MICROKERNEL_SRCS": "defined(__riscv) || defined(__riscv__)",
    "PROD_ARMSIMD32_MICROKERNEL_SRCS": "defined(__arm__)",
    "PROD_FP16ARITH_MICROKERNEL_SRCS": "defined(__arm__)",
    "PROD_NEON_MICROKERNEL_SRCS": "defined(__arm__) || defined(__aarch64__)",
    "PROD_NEONFP16_MICROKERNEL_SRCS": "defined(__arm__) || defined(__aarch64__)",
    "PROD_NEONFMA_MICROKERNEL_SRCS": "defined(__arm__) || defined(__aarch64__)",
    "PROD_NEON_AARCH64_MICROKERNEL_SRCS": "defined(__aarch64__)",
    "PROD_NEONV8_MICROKERNEL_SRCS": "defined(__arm__) || defined(__aarch64__)",
    "PROD_NEONFP16ARITH_MICROKERNEL_SRCS": "defined(__arm__) || defined(__aarch64__)",
    "PROD_NEONFP16ARITH_AARCH64_MICROKERNEL_SRCS": "defined(__aarch64__)",
    "PROD_NEONDOT_MICROKERNEL_SRCS": "defined(__arm__) || defined(__aarch64__)",
    "PROD_NEONDOT_AARCH64_MICROKERNEL_SRCS": "defined(__aarch64__)",
    "PROD_NEONDOTFP16ARITH_MICROKERNEL_SRCS": "defined(__arm__) || defined(__aarch64__)",
    "PROD_NEONDOTFP16ARITH_AARCH64_MICROKERNEL_SRCS": "defined(__aarch64__)",
    "PROD_NEONI8MM_MICROKERNEL_SRCS": "defined(__arm__) || defined(__aarch64__)",
    "PROD_SSE_MICROKERNEL_SRCS": "defined(__i386__) || defined(__i686__) || defined(__x86_64__)",
    "PROD_SSE2_MICROKERNEL_SRCS": "defined(__i386__) || defined(__i686__) || defined(__x86_64__)",
    "PROD_SSSE3_MICROKERNEL_SRCS": "defined(__i386__) || defined(__i686__) || defined(__x86_64__)",
    "PROD_SSE41_MICROKERNEL_SRCS": "defined(__i386__) || defined(__i686__) || defined(__x86_64__)",
    "PROD_AVX_MICROKERNEL_SRCS": "defined(__i386__) || defined(__i686__) || defined(__x86_64__)",
    "PROD_F16C_MICROKERNEL_SRCS": "defined(__i386__) || defined(__i686__) || defined(__x86_64__)",
    "PROD_XOP_MICROKERNEL_SRCS": "defined(__i386__) || defined(__i686__) || defined(__x86_64__)",
    "PROD_FMA3_MICROKERNEL_SRCS": "defined(__i386__) || defined(__i686__) || defined(__x86_64__)",
    "PROD_AVX2_MICROKERNEL_SRCS": "defined(__i386__) || defined(__i686__) || defined(__x86_64__)",
    "PROD_AVX512F_MICROKERNEL_SRCS": "defined(__i386__) || defined(__i686__) || defined(__x86_64__)",
    "PROD_AVX512SKX_MICROKERNEL_SRCS": "defined(__i386__) || defined(__i686__) || defined(__x86_64__)",
    "PROD_AVX512VBMI_MICROKERNEL_SRCS": "defined(__i386__) || defined(__i686__) || defined(__x86_64__)",
    "PROD_AVX512VNNI_MICROKERNEL_SRCS": "defined(__i386__) || defined(__i686__) || defined(__x86_64__)",
    "PROD_AVX512VNNIGFNI_MICROKERNEL_SRCS": "defined(__i386__) || defined(__i686__) || defined(__x86_64__)",
    "PROD_RVV_MICROKERNEL_SRCS": "defined(__riscv) || defined(__riscv__)",
    "PROD_AVXVNNI_MICROKERNEL_SRCS": "defined(__i386__) || defined(__i686__) || defined(__x86_64__)",
    "AARCH32_ASM_MICROKERNEL_SRCS": "defined(__arm__)",
    "AARCH64_ASM_MICROKERNEL_SRCS": "defined(__aarch64__)",

    # add non-prod microkernel sources here:
}

SRC_NAMES = set([
    "OPERATOR_SRCS",
    "SUBGRAPH_SRCS",
    "LOGGING_SRCS",
    "XNNPACK_SRCS",
    "TABLE_SRCS",
    "JIT_SRCS",
    "PROD_SCALAR_MICROKERNEL_SRCS",
    "PROD_FMA_MICROKERNEL_SRCS",
    "PROD_ARMSIMD32_MICROKERNEL_SRCS",
    "PROD_FP16ARITH_MICROKERNEL_SRCS",
    "PROD_NEON_MICROKERNEL_SRCS",
    "PROD_NEONFP16_MICROKERNEL_SRCS",
    "PROD_NEONFMA_MICROKERNEL_SRCS",
    "PROD_NEON_AARCH64_MICROKERNEL_SRCS",
    "PROD_NEONV8_MICROKERNEL_SRCS",
    "PROD_NEONFP16ARITH_MICROKERNEL_SRCS",
    "PROD_NEONFP16ARITH_AARCH64_MICROKERNEL_SRCS",
    "PROD_NEONDOT_MICROKERNEL_SRCS",
    "PROD_NEONDOT_AARCH64_MICROKERNEL_SRCS",
    "PROD_NEONDOTFP16ARITH_MICROKERNEL_SRCS",
    "PROD_NEONDOTFP16ARITH_AARCH64_MICROKERNEL_SRCS",
    "PROD_NEONI8MM_MICROKERNEL_SRCS",
    "PROD_SSE_MICROKERNEL_SRCS",
    "PROD_SSE2_MICROKERNEL_SRCS",
    "PROD_SSSE3_MICROKERNEL_SRCS",
    "PROD_SSE41_MICROKERNEL_SRCS",
    "PROD_AVX_MICROKERNEL_SRCS",
    "PROD_F16C_MICROKERNEL_SRCS",
    "PROD_XOP_MICROKERNEL_SRCS",
    "PROD_FMA3_MICROKERNEL_SRCS",
    "PROD_AVX2_MICROKERNEL_SRCS",
    "PROD_AVX512F_MICROKERNEL_SRCS",
    "PROD_AVX512SKX_MICROKERNEL_SRCS",
    "PROD_AVX512VBMI_MICROKERNEL_SRCS",
    "PROD_AVX512VNNI_MICROKERNEL_SRCS",
    "PROD_AVX512VNNIGFNI_MICROKERNEL_SRCS",
    "PROD_RVV_MICROKERNEL_SRCS",
    "PROD_AVXVNNI_MICROKERNEL_SRCS",
    "AARCH32_ASM_MICROKERNEL_SRCS",
    "AARCH64_ASM_MICROKERNEL_SRCS",

    # add non-prod microkernel sources here:
])

def handle_singleline_parse(line):
    start_index = line.find("(")
    end_index = line.find(")")
    line = line[start_index+1:end_index]
    key_val = line.split(" ")
    return key_val[0], list(map(lambda x: x[4:], key_val[1:]))

def update_sources(xnnpack_path, cmakefile = "XNNPACK/CMakeLists.txt"):
    sources = collections.defaultdict(list)
    with open(os.path.join(xnnpack_path, cmakefile)) as cmake:
        lines = cmake.readlines()
        i = 0
        while i < len(lines):
            line = lines[i]

            if lines[i].startswith("SET") and "src/" in lines[i]:
                name, val = handle_singleline_parse(line)
                sources[name].extend(val)
                i+=1
                continue

            if line.startswith("SET") and line.split('(')[1].strip(' \t\n\r') in set(WRAPPER_SRC_NAMES.keys()) | set(SRC_NAMES):
                name = line.split('(')[1].strip(' \t\n\r')
                i += 1
                while i < len(lines) and len(lines[i]) > 0 and ')' not in lines[i]:
                    # remove "src/" at the beginning, remove whitespaces and newline
                    value = lines[i].strip(' \t\n\r')
                    sources[name].append(value[4:])
                    i += 1
                if i < len(lines) and len(lines[i]) > 4:
                    # remove "src/" at the beginning, possibly ')' at the end
                    value = lines[i].strip(' \t\n\r)')
                    sources[name].append(value[4:])
            else:
                i += 1
    return sources

def gen_wrappers(xnnpack_path):
    xnnpack_sources = collections.defaultdict(list)
    sources = update_sources(xnnpack_path)

    microkernels_sources = update_sources(xnnpack_path, "XNNPACK/cmake/microkernels.cmake")
    for key in  microkernels_sources:
        sources[key] = microkernels_sources[key]

    for name in WRAPPER_SRC_NAMES:
        xnnpack_sources[WRAPPER_SRC_NAMES[name]].extend(sources[name])

    for condition, filenames in xnnpack_sources.items():
        print(condition)
        for filename in filenames:
            filepath = os.path.join(xnnpack_path, "xnnpack_wrappers", filename)

            if not os.path.isdir(os.path.dirname(filepath)):
                os.makedirs(os.path.dirname(filepath))
            with open(filepath, "w") as wrapper:
                print("/* {} */".format(BANNER), file=wrapper)
                print(file=wrapper)

                # Architecture- or platform-dependent preprocessor flags can be
                # defined here. Note: platform_preprocessor_flags can't be used
                # because they are ignored by arc focus & buck project.

                if condition is None:
                    print("#include <%s>" % filename, file=wrapper)
                else:
                    # Include source file only if condition is satisfied
                    print("#if %s" % condition, file=wrapper)
                    print("#include <%s>" % filename, file=wrapper)
                    print("#endif /* %s */" % condition, file=wrapper)

    # update xnnpack_wrapper_defs.bzl file under the same folder
    with open(os.path.join(os.path.dirname(__file__), "xnnpack_wrapper_defs.bzl"), 'w') as wrapper_defs:
        print('"""', file=wrapper_defs)
        print(BANNER, file=wrapper_defs)
        print('"""', file=wrapper_defs)
        for name in WRAPPER_SRC_NAMES:
            print('\n' + name + ' = [', file=wrapper_defs)
            for file_name in sources[name]:
                print('    "xnnpack_wrappers/{}",'.format(file_name), file=wrapper_defs)
            print(']', file=wrapper_defs)

    # update xnnpack_src_defs.bzl file under the same folder
    with open(os.path.join(os.path.dirname(__file__), "xnnpack_src_defs.bzl"), 'w') as src_defs:
        print('"""', file=src_defs)
        print(BANNER, file=src_defs)
        print('"""', file=src_defs)
        for name in SRC_NAMES:
            print('\n' + name + ' = [', file=src_defs)
            for file_name in sources[name]:
                print('    "XNNPACK/src/{}",'.format(file_name), file=src_defs)
            print(']', file=src_defs)


def main(argv):
    if argv is None or len(argv) == 0:
        gen_wrappers(".")
    else:
        gen_wrappers(argv[0])

# The first argument is the place where the "xnnpack_wrappers" folder will be created.
# Run it without arguments will generate "xnnpack_wrappers" in the current path.
# The two .bzl files will always be generated in the current path.
if __name__ == "__main__":
    main(sys.argv[1:])
