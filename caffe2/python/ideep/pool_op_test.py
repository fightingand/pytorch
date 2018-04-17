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

import unittest
import hypothesis.strategies as st
from hypothesis import given, settings, assume
import numpy as np
from caffe2.python import core, workspace
import caffe2.python.hypothesis_test_util as hu
import caffe2.python.ideep_test_util as mu

@unittest.skipIf(not workspace.C.use_ideep, "No IDEEP support.")
class PoolTest(hu.HypothesisTestCase):
    @given(stride=st.integers(1, 3),
           pad=st.integers(0, 3),
           kernel=st.integers(3, 5),
           size=st.integers(7, 9),
           input_channels=st.integers(1, 3),
           batch_size=st.integers(1, 3),
           method=st.sampled_from(["MaxPool", "AveragePool"]),
           **mu.gcs)
    def test_pooling(self, stride, pad, kernel, size,
                         input_channels, batch_size,
                         method, gc, dc):
        assume(pad < kernel)
        op = core.CreateOperator(
            method,
            ["X"],
            ["Y"],
            stride=stride,
            pad=pad,
            kernel=kernel,
        )
        X = np.random.rand(
            batch_size, input_channels, size, size).astype(np.float32)

        self.assertDeviceChecks(dc, op, [X], [0])


if __name__ == "__main__":
    unittest.main()
