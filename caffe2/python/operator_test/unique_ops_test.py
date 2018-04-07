# Copyright (c) 2016-present, Facebook, Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
##############################################################################

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function
from __future__ import unicode_literals

import hypothesis.strategies as st
import numpy as np
from hypothesis import given
from functools import partial

from caffe2.python import core
import caffe2.python.hypothesis_test_util as hu


def _unique_ref(x, return_inverse):
    ret = np.unique(x, return_inverse=return_inverse)
    if not return_inverse:
        ret = [ret]
    return ret


class TestUniqueOps(hu.HypothesisTestCase):
    @given(
        X=hu.tensor1d(
            # allow empty
            min_len=0,
            dtype=np.int32,
            # allow negatives
            elements=st.integers(min_value=-10, max_value=10)),
        return_remapping=st.booleans(),
        **hu.gcs
    )
    def test_unique_op(self, X, return_remapping, gc, dc):
        # impl of unique op does not guarantees return order, sort the input
        # so different impl return same outputs
        X = np.sort(X)

        op = core.CreateOperator(
            "Unique",
            ['X'],
            ["U", "remap"] if return_remapping else ["U"],
        )
        self.assertDeviceChecks(
            device_options=dc,
            op=op,
            inputs=[X],
            outputs_to_check=[0, 1] if return_remapping else [0]
        )
        self.assertReferenceChecks(
            device_option=gc,
            op=op,
            inputs=[X],
            reference=partial(_unique_ref, return_inverse=return_remapping),
        )

    @given(
        X=hu.tensor1d(
            # allow empty
            min_len=0,
            dtype=np.int32,
            # disallow negative
            elements=st.integers(min_value=0, max_value=10)),
        return_remapping=st.booleans(),
        # sparse hash unique op only has cpu impl.
        **hu.gcs_cpu_only
    )
    def test_sparse_hash_unique_op(self, X, return_remapping, gc, dc):
        # impl of unique op does not guarantees return order, sort the input
        # so different impl return same outputs
        X = np.sort(X)

        assert core.IsOperator('Unique_ENGINE_SparseHash')
        op = core.CreateOperator(
            "Unique",
            ["X"],
            ["U", "remap"] if return_remapping else ["U"],
            engine="SparseHash"
        )
        self.assertDeviceChecks(
            device_options=dc,
            op=op,
            inputs=[X],
            outputs_to_check=[0, 1] if return_remapping else [0]
        )
        self.assertReferenceChecks(
            device_option=gc,
            op=op,
            inputs=[X],
            reference=partial(_unique_ref, return_inverse=return_remapping),
        )
