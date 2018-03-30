#include "caffe2/operators/matmul_op.h"

namespace caffe2 {

REGISTER_CPU_OPERATOR(MatMul, MatMulOp<float, CPUContext>);

OPERATOR_SCHEMA(MatMul)
    .NumInputs(2, 3)
    .NumOutputs(1)
    .TensorInferenceFunction([](const OperatorDef& def,
                                const vector<TensorShape>& in) {
      vector<TensorShape> out(1);
      out[0].set_data_type(in[0].data_type());
      ArgumentHelper arg_helper(def);
      int axis_a = arg_helper.GetSingleArgument<int>("axis_a", 1);
      int axis_b = arg_helper.GetSingleArgument<int>("axis_b", 1);
      int trans_a = arg_helper.GetSingleArgument<bool>("trans_a", false);
      int trans_b = arg_helper.GetSingleArgument<bool>("trans_b", false);
      int canonical_axis_a = canonical_axis_index_(axis_a, in[0].dims().size());
      int canonical_axis_b = canonical_axis_index_(axis_b, in[0].dims().size());

      int M = size_to_dim_(canonical_axis_a, GetDimsVector(in[0]));
      int N = size_from_dim_(canonical_axis_b, GetDimsVector(in[1]));
      if (trans_a) {
        M = size_from_dim_(canonical_axis_a, GetDimsVector(in[0]));
      }
      if (trans_b) {
        N = size_to_dim_(canonical_axis_b, GetDimsVector(in[1]));
      }

      out[0].add_dims(M);
      out[0].add_dims(N);

      return out;
    })
    .SetDoc(R"DOC(
Matrix multiplication Y = A * B, where A has size (M x K), B has size (K x N),
and Y will have a size (M x N).
)DOC")
    .Input(0, "A", "2D matrix of size (M x K)")
    .Input(1, "B", "2D matrix of size (K x N)")
    .Output(0, "Y", "2D matrix of size (M x N)")
    .Arg(
        "axis_a",
        "Exclusive axis that divides the first and second dimension \
of matrix A, default to 1")
    .Arg(
        "axis_b",
        "Exclusive axis that divides the first and second dimension \
of matrix B, default to 1")
    .Arg(
        "trans_a",
        "Pass 1 to transpose A before multiplication and after the \
dimension adjustment using axis_a")
    .Arg(
        "trans_b",
        "Pass 1 to transpose B before multiplication and after the \
dimension adjustment using axis_b");

class GetMatMulGradient : public GradientMakerBase {
  using GradientMakerBase::GradientMakerBase;
  vector<OperatorDef> GetGradientDefs() override {
    CAFFE_ENFORCE_EQ(def_.input_size(), 2);

    bool axis_a = 1;
    bool axis_b = 1;
    bool trans_a = 0;
    bool trans_b = 0;

    if (ArgumentHelper::HasArgument(Def(), "trans_a")) {
      trans_a = GetArgument(Def(), "trans_a").i();
    }
    if (ArgumentHelper::HasArgument(Def(), "trans_b")) {
      trans_b = GetArgument(Def(), "trans_b").i();
    }
    if (ArgumentHelper::HasArgument(Def(), "axis_a")) {
      axis_a = GetArgument(Def(), "axis_a").i();
    }
    if (ArgumentHelper::HasArgument(Def(), "axis_b")) {
      axis_b = GetArgument(Def(), "axis_b").i();
    }

    if (trans_a) {
      if (trans_b) {
        // A'B':
        // dA = B'G', dB = G'A'
        return vector<OperatorDef>{
            CreateOperatorDef(
                "MatMul",
                "",
                vector<string>{I(1), GO(0), I(0)},
                vector<string>{GI(0)},
                vector<Argument>{MakeArgument<int>("trans_a", 1),
                                 MakeArgument<int>("trans_b", 1),
                                 MakeArgument<int>("axis_a", axis_b)}),
            CreateOperatorDef(
                "MatMul",
                "",
                vector<string>{GO(0), I(0), I(1)},
                vector<string>{GI(1)},
                vector<Argument>{MakeArgument<int>("trans_a", 1),
                                 MakeArgument<int>("trans_b", 1),
                                 MakeArgument<int>("axis_b", axis_a)})};
      } else {
        // A'B:
        // dA = BG', dB = AG
        return vector<OperatorDef>{
            CreateOperatorDef(
                "MatMul",
                "",
                vector<string>{I(1), GO(0), I(0)},
                vector<string>{GI(0)},
                vector<Argument>{MakeArgument<int>("trans_b", 1),
                                 MakeArgument<int>("axis_a", axis_b)}),
            CreateOperatorDef(
                "MatMul",
                "",
                vector<string>{I(0), GO(0), I(1)},
                vector<string>{GI(1)},
                vector<Argument>{MakeArgument<int>("axis_a", axis_a)})};
      }
    } else {
      if (trans_b) {
        // AB':
        // dA = GB, dB = G'A
        return vector<OperatorDef>{
            CreateOperatorDef(
                "MatMul",
                "",
                vector<string>{GO(0), I(1), I(0)},
                vector<string>{GI(0)},
                vector<Argument>{MakeArgument<int>("axis_b", axis_b)}),
            CreateOperatorDef(
                "MatMul",
                "",
                vector<string>{GO(0), I(0), I(1)},
                vector<string>{GI(1)},
                vector<Argument>{MakeArgument<int>("trans_a", 1),
                                 MakeArgument<int>("axis_b", axis_a)})};
      } else {
        // AB:
        // dA = GB', dB = A'G
        return vector<OperatorDef>{
            CreateOperatorDef(
                "MatMul",
                "",
                vector<string>{GO(0), I(1), I(0)},
                vector<string>{GI(0)},
                vector<Argument>{MakeArgument<int>("trans_b", 1),
                                 MakeArgument<int>("axis_b", axis_b)}),
            CreateOperatorDef(
                "MatMul",
                "",
                vector<string>{I(0), GO(0), I(1)},
                vector<string>{GI(1)},
                vector<Argument>{MakeArgument<int>("trans_a", 1),
                                 MakeArgument<int>("axis_a", axis_a)})};
      }
    }
  }

  bool CopyArguments() const override {
    return false;
  }
};

REGISTER_GRADIENT(MatMul, GetMatMulGradient);

} // namespace caffe2
