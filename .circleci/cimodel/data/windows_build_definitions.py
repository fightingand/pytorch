from collections import OrderedDict

from cimodel.data.pytorch_build_data import TopLevelNode, CONFIG_TREE_DATA
import cimodel.data.dimensions as dimensions
import cimodel.lib.conf_tree as conf_tree
import cimodel.lib.miniutils as miniutils

from dataclasses import dataclass, field
from typing import List, Optional


NON_PR_BRANCH_LIST = [
    "master",
    r"/ci-all\/.*/",
    r"/release\/.*/",
]


class WindowJob:
    def __init__(self,
                 test_index,
                 vscode_spec,
                 cuda_version,
                 force_on_cpu=False):

        self.test_index = test_index
        self.vscode_spec = vscode_spec
        self.cuda_version = cuda_version
        self.force_on_cpu = force_on_cpu
        self.run_on_prs = vscode_spec.year != 2019

    def gen_tree(self):

        base_phase = "build" if self.test_index is None else "test"
        numbered_phase = base_phase if self.test_index is None else base_phase + str(self.test_index)

        key_name = "_".join(["pytorch", "windows", base_phase])

        cpu_forcing_name_parts = ["on", "cpu"] if self.force_on_cpu else []

        target_arch = self.cuda_version if self.cuda_version else "cpu"

        base_name_parts = [
            "pytorch",
            "windows",
            self.vscode_spec.render(),
            "py36",
            target_arch,
        ]

        prerequisite_jobs = []
        if base_phase == "test":
            prerequisite_jobs.append("_".join(base_name_parts + ["build"]))

        arch_env_elements = ["cuda10", "cudnn7"] if self.cuda_version else ["cpu"]

        build_environment_string = "-".join([
            "pytorch",
            "win",
        ] + self.vscode_spec.get_elements() + arch_env_elements + [
            "py3",
        ])

        is_running_on_cuda = bool(self.cuda_version) and not self.force_on_cpu

        vc_product = "Community" if self.vscode_spec.year == 2019 else "BuildTools"

        props_dict = {
            "build_environment": build_environment_string,
            "python_version": "3.6",
            "vc_version": self.vscode_spec.dotted_version(),
            "vc_year": self.vscode_spec.year,
            "vc_product": vc_product,
            "use_cuda": int(is_running_on_cuda),
            "requires": ["setup"] + prerequisite_jobs,
        }

        if self.run_on_prs:
            props_dict["filters"] = {
                "branches": {
                    "only": NON_PR_BRANCH_LIST,
                },
            }

        name_parts = base_name_parts + cpu_forcing_name_parts + [
            numbered_phase,
        ]

        if base_phase == "test":
            test_name = "-".join(["pytorch", "windows", numbered_phase])
            props_dict["test_name"] = test_name

            if is_running_on_cuda:
                props_dict["executor"] = "windows-with-nvidia-gpu"

        props_dict["cuda_version"] = 10 if self.cuda_version else "cpu"
        props_dict["name"] = "_".join(name_parts)

        return [{key_name: props_dict}]


class VcSpec:
    def __init__(self, year, version_elements=[]):
        self.year = year
        self.version_elements = version_elements

    def get_elements(self):
        return [self.prefixed_year()] + self.version_elements

    def dotted_version(self):
        return ".".join(self.version_elements)

    def prefixed_year(self):
        return "vs" + str(self.year)

    def render(self):
        return "_".join(filter(None, [self.prefixed_year(), self.dotted_version()]))


WINDOWS_WORKFLOW_DATA = [
    WindowJob(None, VcSpec(2017, ["14", "11"]), "cuda10.1"),
    WindowJob(1, VcSpec(2017, ["14", "11"]), "cuda10.1"),
    WindowJob(2, VcSpec(2017, ["14", "11"]), "cuda10.1"),
    WindowJob(None, VcSpec(2017, ["14", "16"]), "cuda10.1"),
    WindowJob(1, VcSpec(2017, ["14", "16"]), "cuda10.1"),
    WindowJob(2, VcSpec(2017, ["14", "16"]), "cuda10.1"),
    WindowJob(None, VcSpec(2019), "cuda10.1"),
    WindowJob(1, VcSpec(2019), "cuda10.1"),
    WindowJob(2, VcSpec(2019), "cuda10.1"),
    WindowJob(None, VcSpec(2017, ["14", "11"]), None),
    WindowJob(1, VcSpec(2017, ["14", "11"]), None),
    WindowJob(2, VcSpec(2017, ["14", "11"]), None),
    WindowJob(None, VcSpec(2017, ["14", "16"]), None),
    WindowJob(1, VcSpec(2017, ["14", "16"]), None),
    WindowJob(2, VcSpec(2017, ["14", "16"]), None),
    WindowJob(None, VcSpec(2019), None),
    WindowJob(1, VcSpec(2019), None),
    WindowJob(2, VcSpec(2019), None),
    WindowJob(1, VcSpec(2019), "cuda10.1", force_on_cpu=True),
    WindowJob(2, VcSpec(2019), "cuda10.1", force_on_cpu=True),
]


def get_windows_workflows():
    return [item.gen_tree() for item in WINDOWS_WORKFLOW_DATA]
