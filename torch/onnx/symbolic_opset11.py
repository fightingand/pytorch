import torch
import torch.onnx.symbolic_helper as sym_help
from torch.onnx.symbolic_helper import parse_args, _unimplemented
from torch.onnx.symbolic_helper import _black_list_in_opset


# EDITING THIS FILE? READ THIS FIRST!
# see Note [Edit Symbolic Files] in symbolic_helper.py

# This file exports ONNX ops for opset 11

black_listed_operators = [
    "cumsum", "eq", "ne", "scatter", "clamp", "clamp_min", "clamp_max", "sort", "topk", "hardtanh"
]

for black_listed_op in black_listed_operators:
    vars()[black_listed_op] = _black_list_in_opset(black_listed_op)

@parse_args('v', 'i')
def pixel_shuffle(g, self, upscale_factor):
    dims = self.type().sizes()
    if len(dims) != 4:
        return _unimplemented("pixel_shuffle", "only support 4d input")
    return g.op("DepthToSpace", self, blocksize_i=upscale_factor, mode_s="CRD")


def _interpolate(name, dim, interpolate_mode):
    def symbolic_fn(g, input, output_size, align_corners=None):
        align_corners = sym_help._maybe_get_scalar(align_corners)
        output_size = sym_help._maybe_get_const(output_size, 'is')
        if sym_help._is_value(output_size):
            offsets = g.op("Constant", value_t=torch.tensor([1, 1], dtype=torch.int64))
            output_size = g.op("Concat", offsets, output_size, axis_i=0)
        else:
            output_size = [1 if i < 2 else output_size[-(dim - i)] for i in range(0, dim)]
            output_size = g.op("Constant", value_t=torch.tensor(output_size))
        coordinate_transformation_mode = "asymmetric" if interpolate_mode == "nearest" \
            else "align_corners" if align_corners else "pytorch_half_pixel"
        empty_tensor = g.op("Constant", value_t=torch.tensor([], dtype=torch.float32))
        return g.op("Resize",
                    input,
                    empty_tensor,  # roi only takes effect whith coordinate_transformation_mode="tf_crop_and_resize"
                    empty_tensor,  # scales is not needed since we are sending out_size
                    output_size,
                    coordinate_transformation_mode_s=coordinate_transformation_mode,
                    cubic_coeff_a_f=-0.75,  # only valid when mode="cubic"
                    mode_s=interpolate_mode,  # nearest, linear, or cubic
                    nearest_mode_s="floor")  # only valid when mode="nearest"
    return symbolic_fn


upsample_nearest1d = _interpolate('upsample_nearest1d', 3, "nearest")
upsample_nearest2d = _interpolate('upsample_nearest2d', 4, "nearest")
upsample_nearest3d = _interpolate('upsample_nearest3d', 5, "nearest")
upsample_linear1d = _interpolate('upsample_linear1d', 3, "linear")
upsample_bilinear2d = _interpolate('upsample_bilinear2d', 4, "linear")
upsample_trilinear3d = _interpolate('upsample_trilinear3d', 5, "linear")
upsample_bicubic2d = _interpolate('upsample_bicubic2d', 4, "cubic")
