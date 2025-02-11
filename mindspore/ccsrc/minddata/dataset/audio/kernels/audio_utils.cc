/**
 * Copyright 2021 Huawei Technologies Co., Ltd
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "minddata/dataset/audio/kernels/audio_utils.h"

#include <Eigen/Dense>
#include <fstream>

#include "mindspore/core/base/float16.h"
#include "minddata/dataset/core/type_id.h"
#include "minddata/dataset/kernels/data/data_utils.h"
#include "minddata/dataset/util/random.h"
#include "utils/file_utils.h"

namespace mindspore {
namespace dataset {
/// \brief Generate linearly spaced vector.
/// \param[in] start - Value of the startpoint.
/// \param[in] end - Value of the endpoint.
/// \param[in] n - N points in the output tensor.
/// \param[out] output - Tensor has n points with linearly space. The spacing between the points is (end-start)/(n-1).
/// \return Status return code.
template <typename T>
Status Linspace(std::shared_ptr<Tensor> *output, T start, T end, int n) {
  if (start > end) {
    std::string err = "Linspace: input param end must be greater than start.";
    RETURN_STATUS_UNEXPECTED(err);
  }
  n = std::isnan(n) ? 100 : n;
  TensorShape out_shape({n});
  std::vector<T> linear_vect(n);
  T interval = (n == 1) ? 0 : ((end - start) / (n - 1));
  for (auto i = 0; i < linear_vect.size(); ++i) {
    linear_vect[i] = start + i * interval;
  }
  std::shared_ptr<Tensor> out_t;
  RETURN_IF_NOT_OK(Tensor::CreateFromVector(linear_vect, out_shape, &out_t));
  linear_vect.clear();
  linear_vect.shrink_to_fit();
  *output = out_t;
  return Status::OK();
}

/// \brief Calculate complex tensor angle.
/// \param[in] input - Input tensor, must be complex, <channel, freq, time, complex=2>.
/// \param[out] output - Complex tensor angle.
/// \return Status return code.
template <typename T>
Status ComplexAngle(const std::shared_ptr<Tensor> &input, std::shared_ptr<Tensor> *output) {
  // check complex
  if (!input->IsComplex()) {
    std::string err_msg = "ComplexAngle: input tensor is not in shape of <..., 2>.";
    LOG_AND_RETURN_STATUS_SYNTAX_ERROR(err_msg);
  }
  TensorShape input_shape = input->shape();
  TensorShape out_shape({input_shape[0], input_shape[1], input_shape[2]});
  std::vector<T> phase(input_shape[0] * input_shape[1] * input_shape[2]);
  int ind = 0;

  for (auto itr = input->begin<T>(); itr != input->end<T>(); itr++, ind++) {
    auto x = (*itr);
    itr++;
    auto y = (*itr);
    phase[ind] = atan2(y, x);
  }

  std::shared_ptr<Tensor> out_t;
  RETURN_IF_NOT_OK(Tensor::CreateFromVector(phase, out_shape, &out_t));
  phase.clear();
  phase.shrink_to_fit();
  *output = out_t;
  return Status::OK();
}

/// \brief Calculate complex tensor abs.
/// \param[in] input - Input tensor, must be complex, <channel, freq, time, complex=2>.
/// \param[out] output - Complex tensor abs.
/// \return Status return code.
template <typename T>
Status ComplexAbs(const std::shared_ptr<Tensor> &input, std::shared_ptr<Tensor> *output) {
  // check complex
  if (!input->IsComplex()) {
    std::string err_msg = "ComplexAngle: input tensor is not in shape of <..., 2>.";
    LOG_AND_RETURN_STATUS_SYNTAX_ERROR(err_msg);
  }
  TensorShape input_shape = input->shape();
  TensorShape out_shape({input_shape[0], input_shape[1], input_shape[2]});
  std::vector<T> abs(input_shape[0] * input_shape[1] * input_shape[2]);
  int ind = 0;
  for (auto itr = input->begin<T>(); itr != input->end<T>(); itr++, ind++) {
    T x = (*itr);
    itr++;
    T y = (*itr);
    abs[ind] = sqrt(pow(y, 2) + pow(x, 2));
  }

  std::shared_ptr<Tensor> out_t;
  RETURN_IF_NOT_OK(Tensor::CreateFromVector(abs, out_shape, &out_t));
  *output = out_t;
  return Status::OK();
}

/// \brief Reconstruct complex tensor from norm and angle.
/// \param[in] abs - The absolute value of the complex tensor.
/// \param[in] angle - The angle of the complex tensor.
/// \param[out] output - Complex tensor, <channel, freq, time, complex=2>.
/// \return Status return code.
template <typename T>
Status Polar(const std::shared_ptr<Tensor> &abs, const std::shared_ptr<Tensor> &angle,
             std::shared_ptr<Tensor> *output) {
  // check shape
  if (abs->shape() != angle->shape()) {
    std::string err_msg = "Polar: input tensor shape of abs and angle must be the same.";
    LOG_AND_RETURN_STATUS_SYNTAX_ERROR(err_msg);
  }

  TensorShape input_shape = abs->shape();
  TensorShape out_shape({input_shape[0], input_shape[1], input_shape[2], 2});
  std::vector<T> complex_vec(input_shape[0] * input_shape[1] * input_shape[2] * 2);
  int ind = 0;
  auto itr_abs = abs->begin<T>();
  auto itr_angle = angle->begin<T>();

  for (; itr_abs != abs->end<T>(); itr_abs++, itr_angle++) {
    complex_vec[ind++] = cos(*itr_angle) * (*itr_abs);
    complex_vec[ind++] = sin(*itr_angle) * (*itr_abs);
  }

  std::shared_ptr<Tensor> out_t;
  RETURN_IF_NOT_OK(Tensor::CreateFromVector(complex_vec, out_shape, &out_t));
  *output = out_t;
  return Status::OK();
}

/// \brief Pad complex tensor.
/// \param[in] input - The complex tensor.
/// \param[in] length - The length of padding.
/// \param[in] dim - The dim index for padding.
/// \param[out] output - Complex tensor, <channel, freq, time, complex=2>.
/// \return Status return code.
template <typename T>
Status PadComplexTensor(const std::shared_ptr<Tensor> &input, std::shared_ptr<Tensor> *output, int length, int dim) {
  TensorShape input_shape = input->shape();
  std::vector<int64_t> pad_shape_vec = {input_shape[0], input_shape[1], input_shape[2], input_shape[3]};
  pad_shape_vec[dim] += static_cast<int64_t>(length);
  TensorShape input_shape_with_pad(pad_shape_vec);
  std::vector<T> in_vect(input_shape_with_pad[0] * input_shape_with_pad[1] * input_shape_with_pad[2] *
                         input_shape_with_pad[3]);
  auto itr_input = input->begin<T>();
  int64_t input_cnt = 0;
  /*lint -e{446} ind is modified in the body of the for loop */
  for (int ind = 0; ind < static_cast<int>(in_vect.size()); ind++) {
    in_vect[ind] = (*itr_input);
    input_cnt = (input_cnt + 1) % (input_shape[2] * input_shape[3]);
    itr_input++;
    // complex tensor last dim equals 2, fill zero count equals 2*width
    if (input_cnt == 0 && ind != 0) {
      for (int c = 0; c < length * 2; c++) {
        in_vect[++ind] = 0.0f;
      }
    }
  }
  std::shared_ptr<Tensor> out_t;
  RETURN_IF_NOT_OK(Tensor::CreateFromVector(in_vect, input_shape_with_pad, &out_t));
  *output = out_t;
  return Status::OK();
}

/// \brief Calculate phase.
/// \param[in] angle_0 - The angle.
/// \param[in] angle_1 - The angle.
/// \param[in] phase_advance - The phase advance.
/// \param[in] phase_time0 - The phase at time 0.
/// \param[out] output - Phase tensor.
/// \return Status return code.
template <typename T>
Status Phase(const std::shared_ptr<Tensor> &angle_0, const std::shared_ptr<Tensor> &angle_1,
             const std::shared_ptr<Tensor> &phase_advance, const std::shared_ptr<Tensor> &phase_time0,
             std::shared_ptr<Tensor> *output) {
  TensorShape phase_shape = angle_0->shape();
  std::vector<T> phase(phase_shape[0] * phase_shape[1] * phase_shape[2]);
  auto itr_angle_0 = angle_0->begin<T>();
  auto itr_angle_1 = angle_1->begin<T>();
  auto itr_pa = phase_advance->begin<T>();
  for (int ind = 0, input_cnt = 0; itr_angle_0 != angle_0->end<T>(); itr_angle_0++, itr_angle_1++, ind++) {
    if (ind != 0 && ind % phase_shape[2] == 0) {
      itr_pa++;
      if (itr_pa == phase_advance->end<T>()) {
        itr_pa = phase_advance->begin<T>();
      }
      input_cnt++;
    }
    phase[ind] = (*itr_angle_1) - (*itr_angle_0) - (*itr_pa);
    phase[ind] = phase[ind] - 2 * PI * round(phase[ind] / (2 * PI)) + (*itr_pa);
  }

  // concat phase time 0
  int64_t ind = 0;
  auto itr_p0 = phase_time0->begin<T>();
  (void)phase.insert(phase.begin(), (*itr_p0));
  itr_p0++;
  while (itr_p0 != phase_time0->end<T>()) {
    ind += phase_shape[2];
    phase[ind] = (*itr_p0);
    itr_p0++;
  }
  (void)phase.erase(phase.begin() + static_cast<int>(angle_0->Size()), phase.end());

  // cal phase accum
  for (ind = 0; ind < static_cast<int64_t>(phase.size()); ind++) {
    if (ind % phase_shape[2] != 0) {
      phase[ind] = phase[ind] + phase[ind - 1];
    }
  }
  std::shared_ptr<Tensor> phase_tensor;
  RETURN_IF_NOT_OK(Tensor::CreateFromVector(phase, phase_shape, &phase_tensor));
  *output = phase_tensor;
  return Status::OK();
}

/// \brief Calculate magnitude.
/// \param[in] alphas - The alphas.
/// \param[in] abs_0 - The norm.
/// \param[in] abs_1 - The norm.
/// \param[out] output - Magnitude tensor.
/// \return Status return code.
template <typename T>
Status Mag(const std::shared_ptr<Tensor> &abs_0, const std::shared_ptr<Tensor> &abs_1, std::shared_ptr<Tensor> *output,
           const std::vector<T> &alphas) {
  TensorShape mag_shape = abs_0->shape();
  std::vector<T> mag(mag_shape[0] * mag_shape[1] * mag_shape[2]);
  auto itr_abs_0 = abs_0->begin<T>();
  auto itr_abs_1 = abs_1->begin<T>();
  for (int ind = 0; itr_abs_0 != abs_0->end<T>(); itr_abs_0++, itr_abs_1++, ind++) {
    mag[ind] = alphas[ind % mag_shape[2]] * (*itr_abs_1) + (1 - alphas[ind % mag_shape[2]]) * (*itr_abs_0);
  }
  std::shared_ptr<Tensor> mag_tensor;
  RETURN_IF_NOT_OK(Tensor::CreateFromVector(mag, mag_shape, &mag_tensor));
  *output = mag_tensor;
  return Status::OK();
}

template <typename T>
Status TimeStretch(std::shared_ptr<Tensor> input, std::shared_ptr<Tensor> *output, float rate,
                   std::shared_ptr<Tensor> phase_advance) {
  // pack <..., freq, time, complex>
  TensorShape input_shape = input->shape();
  TensorShape toShape({input->Size() / (input_shape[-1] * input_shape[-2] * input_shape[-3]), input_shape[-3],
                       input_shape[-2], input_shape[-1]});
  RETURN_IF_NOT_OK(input->Reshape(toShape));
  if (rate == 1.0) {
    *output = input;
    return Status::OK();
  }
  // calculate time step and alphas
  std::vector<dsize_t> time_steps_0, time_steps_1;
  std::vector<T> alphas;
  for (int ind = 0;; ind++) {
    auto val = ind * rate;
    if (val >= input_shape[-2]) {
      break;
    }
    int val_int = static_cast<int>(val);
    time_steps_0.push_back(val_int);
    time_steps_1.push_back(val_int + 1);
    alphas.push_back(fmod(val, 1));
  }

  // calculate phase on time 0
  std::shared_ptr<Tensor> spec_time0, phase_time0;
  RETURN_IF_NOT_OK(
    input->Slice(&spec_time0, std::vector<SliceOption>({SliceOption(true), SliceOption(true),
                                                        SliceOption(std::vector<dsize_t>{0}), SliceOption(true)})));
  RETURN_IF_NOT_OK(ComplexAngle<T>(spec_time0, &phase_time0));

  // time pad: add zero to time dim
  RETURN_IF_NOT_OK(PadComplexTensor<T>(input, &input, 2, 2));

  // slice
  std::shared_ptr<Tensor> spec_0;
  RETURN_IF_NOT_OK(input->Slice(&spec_0, std::vector<SliceOption>({SliceOption(true), SliceOption(true),
                                                                   SliceOption(time_steps_0), SliceOption(true)})));
  std::shared_ptr<Tensor> spec_1;
  RETURN_IF_NOT_OK(input->Slice(&spec_1, std::vector<SliceOption>({SliceOption(true), SliceOption(true),
                                                                   SliceOption(time_steps_1), SliceOption(true)})));

  // new slices angle and abs <channel, freq, time>
  std::shared_ptr<Tensor> angle_0, angle_1, abs_0, abs_1;
  RETURN_IF_NOT_OK(ComplexAngle<T>(spec_0, &angle_0));
  RETURN_IF_NOT_OK(ComplexAbs<T>(spec_0, &abs_0));
  RETURN_IF_NOT_OK(ComplexAngle<T>(spec_1, &angle_1));
  RETURN_IF_NOT_OK(ComplexAbs<T>(spec_1, &abs_1));

  // cal phase, there exists precision loss between mindspore and pytorch
  std::shared_ptr<Tensor> phase_tensor;
  RETURN_IF_NOT_OK(Phase<T>(angle_0, angle_1, phase_advance, phase_time0, &phase_tensor));

  // calculate magnitude
  std::shared_ptr<Tensor> mag_tensor;
  RETURN_IF_NOT_OK(Mag<T>(abs_0, abs_1, &mag_tensor, alphas));

  // reconstruct complex from norm and angle
  std::shared_ptr<Tensor> complex_spec_stretch;
  RETURN_IF_NOT_OK(Polar<T>(mag_tensor, phase_tensor, &complex_spec_stretch));

  // unpack
  auto output_shape_vec = input_shape.AsVector();
  output_shape_vec.pop_back();
  output_shape_vec.pop_back();
  output_shape_vec.push_back(complex_spec_stretch->shape()[-2]);
  output_shape_vec.push_back(input_shape[-1]);
  RETURN_IF_NOT_OK(complex_spec_stretch->Reshape(TensorShape(output_shape_vec)));
  *output = complex_spec_stretch;
  return Status::OK();
}

Status TimeStretch(const std::shared_ptr<Tensor> &input, std::shared_ptr<Tensor> *output, float rate, float hop_length,
                   float n_freq) {
  std::shared_ptr<Tensor> phase_advance;
  switch (input->type().value()) {
    case DataType::DE_FLOAT32:
      RETURN_IF_NOT_OK(Linspace<float>(&phase_advance, 0, PI * hop_length, n_freq));
      RETURN_IF_NOT_OK(TimeStretch<float>(input, output, rate, phase_advance));
      break;
    case DataType::DE_FLOAT64:
      RETURN_IF_NOT_OK(Linspace<double>(&phase_advance, 0, PI * hop_length, n_freq));
      RETURN_IF_NOT_OK(TimeStretch<double>(input, output, rate, phase_advance));
      break;
    default:
      RETURN_STATUS_UNEXPECTED("TimeStretch: input tensor type should be float or double, but got: " +
                               input->type().ToString());
  }
  return Status::OK();
}

Status Dct(std::shared_ptr<Tensor> *output, int n_mfcc, int n_mels, NormMode norm) {
  TensorShape dct_shape({n_mels, n_mfcc});
  Tensor::CreateEmpty(dct_shape, DataType(DataType::DE_FLOAT32), output);
  auto iter = (*output)->begin<float>();
  float sqrt_2 = 1 / sqrt(2);
  float sqrt_2_n_mels = sqrt(2.0 / n_mels);
  for (int i = 0; i < n_mels; i++) {
    for (int j = 0; j < n_mfcc; j++) {
      // calculate temp:
      // 1. while norm = None, use 2*cos(PI*(i+0.5)*j/n_mels)
      // 2. while norm = Ortho, divide the first row by sqrt(2),
      //    then using sqrt(2.0 / n_mels)*cos(PI*(i+0.5)*j/n_mels)
      float temp = PI / n_mels * (i + 0.5) * j;
      temp = cos(temp);
      if (norm == NormMode::kOrtho) {
        if (j == 0) {
          temp *= sqrt_2;
        }
        temp *= sqrt_2_n_mels;
      } else {
        temp *= 2;
      }
      (*iter++) = temp;
    }
  }
  return Status::OK();
}

Status RandomMaskAlongAxis(const std::shared_ptr<Tensor> &input, std::shared_ptr<Tensor> *output, int32_t mask_param,
                           float mask_value, int axis, std::mt19937 rnd) {
  std::uniform_int_distribution<int32_t> mask_width_value(0, mask_param);
  TensorShape input_shape = input->shape();
  int32_t mask_dim_size = axis == 1 ? input_shape[-2] : input_shape[-1];
  int32_t mask_width = mask_width_value(rnd);
  std::uniform_int_distribution<int32_t> min_freq_value(0, mask_dim_size - mask_width);
  int32_t mask_start = min_freq_value(rnd);

  return MaskAlongAxis(input, output, mask_width, mask_start, mask_value, axis);
}

Status MaskAlongAxis(const std::shared_ptr<Tensor> &input, std::shared_ptr<Tensor> *output, int32_t mask_width,
                     int32_t mask_start, float mask_value, int32_t axis) {
  if (axis != 2 && axis != 1) {
    RETURN_STATUS_UNEXPECTED("MaskAlongAxis: only support Time and Frequency masking, axis should be 1 or 2.");
  }
  TensorShape input_shape = input->shape();
  // squeeze input
  TensorShape squeeze_shape = TensorShape({-1, input_shape[-2], input_shape[-1]});
  (void)input->Reshape(squeeze_shape);

  int check_dim_ind = (axis == 1) ? -2 : -1;
  CHECK_FAIL_RETURN_UNEXPECTED(0 <= mask_start && mask_start <= input_shape[check_dim_ind],
                               "MaskAlongAxis: mask_start should be less than the length of chosen dimension.");
  CHECK_FAIL_RETURN_UNEXPECTED(mask_start + mask_width <= input_shape[check_dim_ind],
                               "MaskAlongAxis: the sum of mask_start and mask_width is out of bounds.");

  int32_t cell_size = input->type().SizeInBytes();

  if (axis == 1) {
    // freq
    for (int ind = 0; ind < input->Size() / input_shape[-2] * mask_width; ind++) {
      int block_num = ind / (mask_width * input_shape[-1]);
      auto start_pos = ind % (mask_width * input_shape[-1]) + mask_start * input_shape[-1] +
                       input_shape[-1] * input_shape[-2] * block_num;
      auto start_mem_pos = const_cast<uchar *>(input->GetBuffer() + start_pos * cell_size);
      if (input->type() != DataType::DE_FLOAT64) {
        // tensor float 32
        auto mask_val = static_cast<float>(mask_value);
        CHECK_FAIL_RETURN_UNEXPECTED(memcpy_s(start_mem_pos, cell_size, &mask_val, cell_size) == 0,
                                     "MaskAlongAxis: mask failed, memory copy error.");
      } else {
        // tensor float 64
        CHECK_FAIL_RETURN_UNEXPECTED(memcpy_s(start_mem_pos, cell_size, &mask_value, cell_size) == 0,
                                     "MaskAlongAxis: mask failed, memory copy error.");
      }
    }
  } else {
    // time
    for (int ind = 0; ind < input->Size() / input_shape[-1] * mask_width; ind++) {
      int row_num = ind / mask_width;
      auto start_pos = ind % mask_width + mask_start + input_shape[-1] * row_num;
      auto start_mem_pos = const_cast<uchar *>(input->GetBuffer() + start_pos * cell_size);
      if (input->type() != DataType::DE_FLOAT64) {
        // tensor float 32
        auto mask_val = static_cast<float>(mask_value);
        CHECK_FAIL_RETURN_UNEXPECTED(memcpy_s(start_mem_pos, cell_size, &mask_val, cell_size) == 0,
                                     "MaskAlongAxis: mask failed, memory copy error.");
      } else {
        // tensor float 64
        CHECK_FAIL_RETURN_UNEXPECTED(memcpy_s(start_mem_pos, cell_size, &mask_value, cell_size) == 0,
                                     "MaskAlongAxis: mask failed, memory copy error.");
      }
    }
  }
  // unsqueeze input
  (void)input->Reshape(input_shape);
  *output = input;
  return Status::OK();
}

template <typename T>
Status Norm(const std::shared_ptr<Tensor> &input, std::shared_ptr<Tensor> *output, float power) {
  // calculate the output dimension
  auto input_size = input->shape().AsVector();
  int32_t dim_back = static_cast<int32_t>(input_size.back());
  CHECK_FAIL_RETURN_UNEXPECTED(
    dim_back == 2, "ComplexNorm: expect complex input of shape <..., 2>, but got: " + std::to_string(dim_back));
  input_size.pop_back();
  TensorShape out_shape = TensorShape(input_size);
  RETURN_IF_NOT_OK(Tensor::CreateEmpty(out_shape, input->type(), output));

  // calculate norm, using: .pow(2.).sum(-1).pow(0.5 * power)
  auto itr_out = (*output)->begin<T>();
  auto itr_in = input->begin<T>();

  for (; itr_out != (*output)->end<T>(); ++itr_out) {
    auto a = static_cast<T>(*itr_in);
    ++itr_in;
    auto b = static_cast<T>(*itr_in);
    ++itr_in;
    auto res = pow(a, 2) + pow(b, 2);
    *itr_out = static_cast<T>(pow(res, (0.5 * power)));
  }

  return Status::OK();
}

Status ComplexNorm(const std::shared_ptr<Tensor> &input, std::shared_ptr<Tensor> *output, float power) {
  try {
    if (input->type().value() >= DataType::DE_INT8 && input->type().value() <= DataType::DE_FLOAT16) {
      // convert the data type to float
      std::shared_ptr<Tensor> input_tensor;
      RETURN_IF_NOT_OK(TypeCast(input, &input_tensor, DataType(DataType::DE_FLOAT32)));

      RETURN_IF_NOT_OK(Norm<float>(input_tensor, output, power));
    } else if (input->type().value() == DataType::DE_FLOAT32) {
      RETURN_IF_NOT_OK(Norm<float>(input, output, power));
    } else if (input->type().value() == DataType::DE_FLOAT64) {
      RETURN_IF_NOT_OK(Norm<double>(input, output, power));
    } else {
      RETURN_STATUS_UNEXPECTED("ComplexNorm: input tensor type should be int, float or double, but got: " +
                               input->type().ToString());
    }
    return Status::OK();
  } catch (std::runtime_error &e) {
    RETURN_STATUS_UNEXPECTED("ComplexNorm: " + std::string(e.what()));
  }
}

template <typename T>
float sgn(T val) {
  return static_cast<float>(static_cast<T>(0) < val) - static_cast<float>(val < static_cast<T>(0));
}

template <typename T>
Status Decoding(const std::shared_ptr<Tensor> &input, std::shared_ptr<Tensor> *output, T mu) {
  RETURN_IF_NOT_OK(Tensor::CreateEmpty(input->shape(), input->type(), output));
  auto itr_out = (*output)->begin<T>();
  auto itr = input->begin<T>();
  auto end = input->end<T>();

  while (itr != end) {
    auto x_mu = *itr;
    CHECK_FAIL_RETURN_SYNTAX_ERROR(mu != 0, "mu can not be zero.");
    x_mu = ((x_mu) / mu) * 2 - 1.0;
    x_mu = sgn(x_mu) * expm1(fabs(x_mu) * log1p(mu)) / mu;
    *itr_out = x_mu;
    ++itr_out;
    ++itr;
  }
  return Status::OK();
}

Status MuLawDecoding(const std::shared_ptr<Tensor> &input, std::shared_ptr<Tensor> *output,
                     int32_t quantization_channels) {
  if (input->type().IsInt() || input->type() == DataType(DataType::DE_FLOAT16) ||
      input->type() == DataType(DataType::DE_FLOAT32)) {
    float f_mu = static_cast<float>(quantization_channels) - 1;

    // convert the data type to float
    std::shared_ptr<Tensor> input_tensor;
    RETURN_IF_NOT_OK(TypeCast(input, &input_tensor, DataType(DataType::DE_FLOAT32)));

    RETURN_IF_NOT_OK(Decoding<float>(input_tensor, output, f_mu));
  } else if (input->type() == DataType(DataType::DE_FLOAT64)) {
    double f_mu = static_cast<double>(quantization_channels) - 1;

    RETURN_IF_NOT_OK(Decoding<double>(input, output, f_mu));
  } else {
    RETURN_STATUS_UNEXPECTED("MuLawDecoding: input tensor type should be int, float or double, but got: " +
                             input->type().ToString());
  }
  return Status::OK();
}

template <typename T>
Status Encoding(const std::shared_ptr<Tensor> &input, std::shared_ptr<Tensor> *output, T mu) {
  RETURN_IF_NOT_OK(Tensor::CreateEmpty(input->shape(), DataType(DataType::DE_INT32), output));
  auto itr_out = (*output)->begin<int32_t>();
  auto itr = input->begin<T>();
  auto end = input->end<T>();

  while (itr != end) {
    auto x = *itr;
    x = sgn(x) * log1p(mu * fabs(x)) / log1p(mu);
    x = (x + 1) / 2 * mu + 0.5;
    *itr_out = static_cast<int32_t>(x);
    ++itr_out;
    ++itr;
  }
  return Status::OK();
}

Status MuLawEncoding(const std::shared_ptr<Tensor> &input, std::shared_ptr<Tensor> *output,
                     int32_t quantization_channels) {
  if (input->type().IsInt() || input->type() == DataType(DataType::DE_FLOAT16)) {
    float f_mu = static_cast<float>(quantization_channels) - 1;

    // convert the data type to float
    std::shared_ptr<Tensor> input_tensor;
    RETURN_IF_NOT_OK(TypeCast(input, &input_tensor, DataType(DataType::DE_FLOAT32)));

    RETURN_IF_NOT_OK(Encoding<float>(input_tensor, output, f_mu));
  } else if (input->type() == DataType(DataType::DE_FLOAT32)) {
    float f_mu = static_cast<float>(quantization_channels) - 1;

    RETURN_IF_NOT_OK(Encoding<float>(input, output, f_mu));
  } else if (input->type() == DataType(DataType::DE_FLOAT64)) {
    double f_mu = static_cast<double>(quantization_channels) - 1;

    RETURN_IF_NOT_OK(Encoding<double>(input, output, f_mu));
  } else {
    RETURN_STATUS_UNEXPECTED("MuLawEncoding: input tensor type should be int, float or double, but got: " +
                             input->type().ToString());
  }
  return Status::OK();
}

template <typename T>
Status FadeIn(std::shared_ptr<Tensor> *output, int32_t fade_in_len, FadeShape fade_shape) {
  T start = 0;
  T end = 1;
  RETURN_IF_NOT_OK(Linspace<T>(output, start, end, fade_in_len));
  for (auto iter = (*output)->begin<T>(); iter != (*output)->end<T>(); iter++) {
    switch (fade_shape) {
      case FadeShape::kLinear:
        break;
      case FadeShape::kExponential:
        // Compute the scale factor of the exponential function, pow(2.0, *in_ter - 1.0) * (*in_ter)
        *iter = static_cast<T>(std::pow(2.0, *iter - 1.0) * (*iter));
        break;
      case FadeShape::kLogarithmic:
        // Compute the scale factor of the logarithmic function, log(*in_iter + 0.1) + 1.0
        *iter = static_cast<T>(std::log10(*iter + 0.1) + 1.0);
        break;
      case FadeShape::kQuarterSine:
        // Compute the scale factor of the quarter_sine function, sin((*in_iter - 1.0) * PI / 2.0)
        *iter = static_cast<T>(std::sin((*iter) * PI / 2.0));
        break;
      case FadeShape::kHalfSine:
        // Compute the scale factor of the half_sine function, sin((*in_iter) * PI - PI / 2.0) / 2.0 + 0.5
        *iter = static_cast<T>(std::sin((*iter) * PI - PI / 2.0) / 2.0 + 0.5);
        break;
    }
  }
  return Status::OK();
}

template <typename T>
Status FadeOut(std::shared_ptr<Tensor> *output, int32_t fade_out_len, FadeShape fade_shape) {
  T start = 0;
  T end = 1;
  RETURN_IF_NOT_OK(Linspace<T>(output, start, end, fade_out_len));
  for (auto iter = (*output)->begin<T>(); iter != (*output)->end<T>(); iter++) {
    switch (fade_shape) {
      case FadeShape::kLinear:
        // In fade out, invert *out_iter
        *iter = static_cast<T>(1.0 - *iter);
        break;
      case FadeShape::kExponential:
        // Compute the scale factor of the exponential function
        *iter = static_cast<T>(std::pow(2.0, -*iter) * (1.0 - *iter));
        break;
      case FadeShape::kLogarithmic:
        // Compute the scale factor of the logarithmic function
        *iter = static_cast<T>(std::log10(1.1 - *iter) + 1.0);
        break;
      case FadeShape::kQuarterSine:
        // Compute the scale factor of the quarter_sine function
        *iter = static_cast<T>(std::sin((*iter) * PI / 2.0 + PI / 2.0));
        break;
      case FadeShape::kHalfSine:
        // Compute the scale factor of the half_sine function
        *iter = static_cast<T>(std::sin((*iter) * PI + PI / 2.0) / 2.0 + 0.5);
        break;
    }
  }
  return Status::OK();
}

template <typename T>
Status Fade(const std::shared_ptr<Tensor> &input, std::shared_ptr<Tensor> *output, int32_t fade_in_len,
            int32_t fade_out_len, FadeShape fade_shape) {
  RETURN_IF_NOT_OK(Tensor::CreateFromTensor(input, output));
  const TensorShape input_shape = input->shape();
  int32_t waveform_length = static_cast<int32_t>(input_shape[-1]);
  CHECK_FAIL_RETURN_UNEXPECTED(fade_in_len <= waveform_length, "Fade: fade_in_len exceeds waveform length.");
  CHECK_FAIL_RETURN_UNEXPECTED(fade_out_len <= waveform_length, "Fade: fade_out_len exceeds waveform length.");
  int32_t num_waveform = static_cast<int32_t>(input->Size() / waveform_length);
  TensorShape toShape = TensorShape({num_waveform, waveform_length});
  RETURN_IF_NOT_OK((*output)->Reshape(toShape));
  TensorPtr fade_in;
  RETURN_IF_NOT_OK(FadeIn<T>(&fade_in, fade_in_len, fade_shape));
  TensorPtr fade_out;
  RETURN_IF_NOT_OK(FadeOut<T>(&fade_out, fade_out_len, fade_shape));

  // Add fade in to input tensor
  auto output_iter = (*output)->begin<T>();
  for (auto fade_in_iter = fade_in->begin<T>(); fade_in_iter != fade_in->end<T>(); fade_in_iter++) {
    *output_iter = (*output_iter) * (*fade_in_iter);
    for (int32_t j = 1; j < num_waveform; j++) {
      output_iter += waveform_length;
      *output_iter = (*output_iter) * (*fade_in_iter);
    }
    output_iter -= ((num_waveform - 1) * waveform_length);
    ++output_iter;
  }

  // Add fade out to input tensor
  output_iter = (*output)->begin<T>();
  output_iter += (waveform_length - fade_out_len);
  for (auto fade_out_iter = fade_out->begin<T>(); fade_out_iter != fade_out->end<T>(); fade_out_iter++) {
    *output_iter = (*output_iter) * (*fade_out_iter);
    for (int32_t j = 1; j < num_waveform; j++) {
      output_iter += waveform_length;
      *output_iter = (*output_iter) * (*fade_out_iter);
    }
    output_iter -= ((num_waveform - 1) * waveform_length);
    ++output_iter;
  }
  (*output)->Reshape(input_shape);
  return Status::OK();
}

Status Fade(const std::shared_ptr<Tensor> &input, std::shared_ptr<Tensor> *output, int32_t fade_in_len,
            int32_t fade_out_len, FadeShape fade_shape) {
  if (DataType::DE_INT8 <= input->type().value() && input->type().value() <= DataType::DE_FLOAT32) {
    std::shared_ptr<Tensor> waveform;
    RETURN_IF_NOT_OK(TypeCast(input, &waveform, DataType(DataType::DE_FLOAT32)));
    RETURN_IF_NOT_OK(Fade<float>(waveform, output, fade_in_len, fade_out_len, fade_shape));
  } else if (input->type().value() == DataType::DE_FLOAT64) {
    RETURN_IF_NOT_OK(Fade<double>(input, output, fade_in_len, fade_out_len, fade_shape));
  } else {
    RETURN_STATUS_UNEXPECTED("Fade: input tensor type should be int, float or double, but got: " +
                             input->type().ToString());
  }
  return Status::OK();
}

Status Magphase(const TensorRow &input, TensorRow *output, float power) {
  std::shared_ptr<Tensor> mag;
  std::shared_ptr<Tensor> phase;

  RETURN_IF_NOT_OK(ComplexNorm(input[0], &mag, power));
  if (input[0]->type() == DataType(DataType::DE_FLOAT64)) {
    RETURN_IF_NOT_OK(Angle<double>(input[0], &phase));
  } else {
    std::shared_ptr<Tensor> tmp;
    RETURN_IF_NOT_OK(TypeCast(input[0], &tmp, DataType(DataType::DE_FLOAT32)));
    RETURN_IF_NOT_OK(Angle<float>(tmp, &phase));
  }
  (*output).push_back(mag);
  (*output).push_back(phase);

  return Status::OK();
}

Status MedianSmoothing(const std::shared_ptr<Tensor> &input, std::shared_ptr<Tensor> *output, int32_t win_length) {
  auto channel = input->shape()[0];
  auto num_of_frames = input->shape()[1];
  // Centered windowed
  int32_t pad_length = (win_length - 1) / 2;
  int32_t out_length = num_of_frames + pad_length - win_length + 1;
  TensorShape out_shape({channel, out_length});
  std::vector<int> signal;
  std::vector<int> out;
  std::vector<int> indices(channel * (num_of_frames + pad_length), 0);
  // "replicate" padding in any dimension
  for (auto itr = input->begin<int>(); itr != input->end<int>(); ++itr) {
    signal.push_back(*itr);
  }
  for (int i = 0; i < channel; ++i) {
    for (int j = 0; j < pad_length; ++j) {
      indices[i * (num_of_frames + pad_length) + j] = signal[i * num_of_frames];
    }
  }
  for (int i = 0; i < channel; ++i) {
    for (int j = 0; j < num_of_frames; ++j) {
      indices[i * (num_of_frames + pad_length) + j + pad_length] = signal[i * num_of_frames + j];
    }
  }
  for (int i = 0; i < channel; ++i) {
    int32_t index = i * (num_of_frames + pad_length);
    for (int j = 0; j < out_length; ++j) {
      std::vector<int> tem(indices.begin() + index, indices.begin() + win_length + index);
      std::sort(tem.begin(), tem.end());
      out.push_back(tem[pad_length]);
      ++index;
    }
  }
  RETURN_IF_NOT_OK(Tensor::CreateFromVector(out, out_shape, output));
  return Status::OK();
}

Status DetectPitchFrequency(const std::shared_ptr<Tensor> &input, std::shared_ptr<Tensor> *output, int32_t sample_rate,
                            float frame_time, int32_t win_length, int32_t freq_low, int32_t freq_high) {
  std::shared_ptr<Tensor> nccf;
  std::shared_ptr<Tensor> indices;
  std::shared_ptr<Tensor> smooth_indices;
  // pack batch
  TensorShape input_shape = input->shape();
  TensorShape to_shape({input->Size() / input_shape[-1], input_shape[-1]});
  RETURN_IF_NOT_OK(input->Reshape(to_shape));
  if (input->type() == DataType(DataType::DE_FLOAT32)) {
    RETURN_IF_NOT_OK(ComputeNccf<float>(input, &nccf, sample_rate, frame_time, freq_low));
    RETURN_IF_NOT_OK(FindMaxPerFrame<float>(nccf, &indices, sample_rate, freq_high));
  } else if (input->type() == DataType(DataType::DE_FLOAT64)) {
    RETURN_IF_NOT_OK(ComputeNccf<double>(input, &nccf, sample_rate, frame_time, freq_low));
    RETURN_IF_NOT_OK(FindMaxPerFrame<double>(nccf, &indices, sample_rate, freq_high));
  } else {
    RETURN_IF_NOT_OK(ComputeNccf<float16>(input, &nccf, sample_rate, frame_time, freq_low));
    RETURN_IF_NOT_OK(FindMaxPerFrame<float16>(nccf, &indices, sample_rate, freq_high));
  }
  RETURN_IF_NOT_OK(MedianSmoothing(indices, &smooth_indices, win_length));

  // Convert indices to frequency
  constexpr double EPSILON = 1e-9;
  TensorShape freq_shape = smooth_indices->shape();
  std::vector<float> out;
  for (auto itr_fre = smooth_indices->begin<int>(); itr_fre != smooth_indices->end<int>(); ++itr_fre) {
    out.push_back(sample_rate / (EPSILON + *itr_fre));
  }

  // unpack batch
  auto shape_vec = input_shape.AsVector();
  shape_vec[shape_vec.size() - 1] = freq_shape[-1];
  TensorShape out_shape(shape_vec);
  RETURN_IF_NOT_OK(Tensor::CreateFromVector(out, out_shape, output));
  return Status::OK();
}

Status GenerateWaveTable(std::shared_ptr<Tensor> *output, const DataType &type, Modulation modulation,
                         int32_t table_size, float min, float max, float phase) {
  RETURN_UNEXPECTED_IF_NULL(output);
  int32_t phase_offset = static_cast<int32_t>(phase / PI / 2 * table_size + 0.5);
  // get the offset of the i-th
  std::vector<int32_t> point;
  for (auto i = 0; i < table_size; i++) {
    point.push_back((i + phase_offset) % table_size);
  }

  std::shared_ptr<Tensor> wave_table;
  RETURN_IF_NOT_OK(Tensor::CreateEmpty(TensorShape({table_size}), DataType(DataType::DE_FLOAT32), &wave_table));

  auto iter = wave_table->begin<float>();

  if (modulation == Modulation::kSinusoidal) {
    for (int i = 0; i < table_size; iter++, i++) {
      // change phase
      *iter = (sin(point[i] * PI / table_size * 2) + 1) / 2;
    }
  } else {
    for (int i = 0; i < table_size; iter++, i++) {
      // change phase
      *iter = point[i] * 2.0 / table_size;
      // get complete offset
      int32_t value = static_cast<int>(4 * point[i] / table_size);
      // change the value of the square wave according to the number of complete offsets
      if (value == 0) {
        *iter = *iter + 0.5;
      } else if (value == 1 || value == 2) {
        *iter = 1.5 - *iter;
      } else if (value == 3) {
        *iter = *iter - 1.5;
      }
    }
  }
  for (iter = wave_table->begin<float>(); iter != wave_table->end<float>(); iter++) {
    *iter = *iter * (max - min) + min;
  }
  if (type.IsInt()) {
    for (iter = wave_table->begin<float>(); iter != wave_table->end<float>(); iter++) {
      if (*iter < 0) {
        *iter = *iter - 0.5;
      } else {
        *iter = *iter + 0.5;
      }
    }
    RETURN_IF_NOT_OK(TypeCast(wave_table, output, DataType(DataType::DE_INT32)));
  } else if (type.IsFloat()) {
    RETURN_IF_NOT_OK(TypeCast(wave_table, output, DataType(DataType::DE_FLOAT32)));
  }

  return Status::OK();
}

Status ReadWaveFile(const std::string &wav_file_dir, std::vector<float> *waveform_vec, int32_t *sample_rate) {
  RETURN_UNEXPECTED_IF_NULL(waveform_vec);
  RETURN_UNEXPECTED_IF_NULL(sample_rate);
  auto wav_realpath = FileUtils::GetRealPath(wav_file_dir.data());
  if (!wav_realpath.has_value()) {
    MS_LOG(ERROR) << "Invalid file, get real path failed, path=" << wav_file_dir;
    RETURN_STATUS_UNEXPECTED("Invalid file, get real path failed, path=" + wav_file_dir);
  }

  const float kMaxVal = 32767.0;
  const int kDataMove = 2;
  Path file_path(wav_realpath.value());
  CHECK_FAIL_RETURN_UNEXPECTED(file_path.Exists() && !file_path.IsDirectory(),
                               "Invalid file, failed to find metadata file:" + file_path.ToString());
  std::ifstream in(file_path.ToString(), std::ios::in | std::ios::binary);
  CHECK_FAIL_RETURN_UNEXPECTED(in.is_open(), "Invalid file, failed to open metadata file:" + file_path.ToString() +
                                               ", make sure the file not damaged or permission denied.");
  WavHeader *header = new WavHeader();
  in.read(reinterpret_cast<char *>(header), sizeof(WavHeader));
  *sample_rate = header->sampleRate;
  std::unique_ptr<char[]> data = std::make_unique<char[]>(header->subChunk2Size);
  in.read(data.get(), header->subChunk2Size);
  float bytesPerSample = header->bitsPerSample / 8;
  if (bytesPerSample == 0) {
    in.close();
    delete header;
    return Status(StatusCode::kMDUnexpectedError, __LINE__, __FILE__, "ReadWaveFile: divide zero error.");
  }
  int numSamples = header->subChunk2Size / bytesPerSample;
  waveform_vec->resize(numSamples);
  for (int i = 0; i < numSamples; i++) {
    (*waveform_vec)[i] = static_cast<int16_t>(data[kDataMove * i] / kMaxVal);
  }
  in.close();
  delete header;
  return Status::OK();
}

Status ComputeCmnStartAndEnd(int32_t cmn_window, int32_t min_cmn_window, bool center, int32_t idx, int32_t num_frames,
                             int32_t *cmn_window_start_p, int32_t *cmn_window_end_p) {
  RETURN_UNEXPECTED_IF_NULL(cmn_window_start_p);
  RETURN_UNEXPECTED_IF_NULL(cmn_window_end_p);
  CHECK_FAIL_RETURN_UNEXPECTED(
    cmn_window >= 0, "SlidingWindowCmn: cmn_window must be non negative, but got: " + std::to_string(cmn_window));
  CHECK_FAIL_RETURN_UNEXPECTED(min_cmn_window >= 0, "SlidingWindowCmn: min_cmn_window must be non negative, but got: " +
                                                      std::to_string(min_cmn_window));
  int32_t cmn_window_start = 0, cmn_window_end = 0;
  constexpr int window_center = 2;
  if (center) {
    cmn_window_start = idx - cmn_window / window_center;
    cmn_window_end = cmn_window_start + cmn_window;
  } else {
    cmn_window_start = idx - cmn_window;
    cmn_window_end = idx + 1;
  }
  if (cmn_window_start < 0) {
    cmn_window_end -= cmn_window_start;
    cmn_window_start = 0;
  }
  if (!center) {
    if (cmn_window_end > idx) {
      cmn_window_end = std::max(idx + 1, min_cmn_window);
    }
  }
  if (cmn_window_end > num_frames) {
    cmn_window_start -= (cmn_window_end - num_frames);
    cmn_window_end = num_frames;
    if (cmn_window_start < 0) {
      cmn_window_start = 0;
    }
  }

  *cmn_window_start_p = cmn_window_start;
  *cmn_window_end_p = cmn_window_end;
  return Status::OK();
}

template <typename T>
Status ComputeCmnWaveform(const std::shared_ptr<Tensor> &input, std::shared_ptr<Tensor> *cmn_waveform_p,
                          int32_t num_channels, int32_t num_frames, int32_t num_feats, int32_t cmn_window,
                          int32_t min_cmn_window, bool center, bool norm_vars) {
  using ArrayXT = Eigen::Array<T, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>;
  constexpr int square_num = 2;
  int32_t last_window_start = -1, last_window_end = -1;
  ArrayXT cur_sum = ArrayXT(num_channels, num_feats);
  ArrayXT cur_sum_sq;
  if (norm_vars) {
    cur_sum_sq = ArrayXT(num_channels, num_feats);
  }
  for (int i = 0; i < num_frames; ++i) {
    int32_t cmn_window_start = 0, cmn_window_end = 0;
    RETURN_IF_NOT_OK(
      ComputeCmnStartAndEnd(cmn_window, min_cmn_window, center, i, num_frames, &cmn_window_start, &cmn_window_end));
    int32_t row = cmn_window_end - cmn_window_start * 2;
    int32_t cmn_window_frames = cmn_window_end - cmn_window_start;
    for (int32_t m = 0; m < num_channels; ++m) {
      if (last_window_start == -1) {
        auto it = reinterpret_cast<T *>(const_cast<uchar *>(input->GetBuffer()));
        it += (m * num_frames * num_feats + cmn_window_start * num_feats);
        auto tmp_map = Eigen::Map<ArrayXT>(it, row, num_feats);
        if (i > 0) {
          cur_sum.row(m) += tmp_map.colwise().sum();
          if (norm_vars) {
            cur_sum_sq.row(m) += tmp_map.pow(square_num).colwise().sum();
          }
        } else {
          cur_sum.row(m) = tmp_map.colwise().sum();
          if (norm_vars) {
            cur_sum_sq.row(m) = tmp_map.pow(square_num).colwise().sum();
          }
        }
      } else {
        if (cmn_window_start > last_window_start) {
          auto it = reinterpret_cast<T *>(const_cast<uchar *>(input->GetBuffer()));
          it += (m * num_frames * num_feats + last_window_start * num_feats);
          auto tmp_map = Eigen::Map<ArrayXT>(it, 1, num_feats);
          cur_sum.row(m) -= tmp_map;
          if (norm_vars) {
            cur_sum_sq.row(m) -= tmp_map.pow(square_num);
          }
        }
        if (cmn_window_end > last_window_end) {
          auto it = reinterpret_cast<T *>(const_cast<uchar *>(input->GetBuffer()));
          it += (m * num_frames * num_feats + last_window_end * num_feats);
          auto tmp_map = Eigen::Map<ArrayXT>(it, 1, num_feats);
          cur_sum.row(m) += tmp_map;
          if (norm_vars) {
            cur_sum_sq.row(m) += tmp_map.pow(square_num);
          }
        }
      }

      auto it = reinterpret_cast<T *>(const_cast<uchar *>(input->GetBuffer()));
      auto cmn_it = reinterpret_cast<T *>(const_cast<uchar *>((*cmn_waveform_p)->GetBuffer()));
      it += (m * num_frames * num_feats + i * num_feats);
      cmn_it += (m * num_frames * num_feats + i * num_feats);
      Eigen::Map<ArrayXT>(cmn_it, 1, num_feats) =
        Eigen::Map<ArrayXT>(it, 1, num_feats) - cur_sum.row(m) / cmn_window_frames;
      if (norm_vars) {
        if (cmn_window_frames == 1) {
          auto cmn_it_1 = reinterpret_cast<T *>(const_cast<uchar *>((*cmn_waveform_p)->GetBuffer()));
          cmn_it_1 += (m * num_frames * num_feats + i * num_feats);
          Eigen::Map<ArrayXT>(cmn_it_1, 1, num_feats).setZero();
        } else {
          auto variance = (Eigen::Map<ArrayXT>(cur_sum_sq.data(), num_channels, num_feats) / cmn_window_frames) -
                          (cur_sum.pow(2) / std::pow(cmn_window_frames, 2));
          auto cmn_it_2 = reinterpret_cast<T *>(const_cast<uchar *>((*cmn_waveform_p)->GetBuffer()));
          cmn_it_2 += (m * num_frames * num_feats + i * num_feats);
          Eigen::Map<ArrayXT>(cmn_it_2, 1, num_feats) =
            Eigen::Map<ArrayXT>(cmn_it_2, 1, num_feats) * (1 / variance.sqrt()).row(m);
        }
      }
    }
    last_window_start = cmn_window_start;
    last_window_end = cmn_window_end;
  }
  return Status::OK();
}

template <typename T>
Status SlidingWindowCmnHelper(const std::shared_ptr<Tensor> &input, std::shared_ptr<Tensor> *output, int32_t cmn_window,
                              int32_t min_cmn_window, bool center, bool norm_vars) {
  int32_t num_frames = input->shape()[Tensor::HandleNeg(-2, input->shape().Size())];
  int32_t num_feats = input->shape()[Tensor::HandleNeg(-1, input->shape().Size())];

  int32_t first_index = 1;
  std::vector<dsize_t> input_shape = input->shape().AsVector();
  std::for_each(input_shape.begin(), input_shape.end(), [&first_index](const dsize_t &item) { first_index *= item; });
  RETURN_IF_NOT_OK(
    input->Reshape(TensorShape({static_cast<int>(first_index / (num_frames * num_feats)), num_frames, num_feats})));

  int32_t num_channels = static_cast<int32_t>(input->shape()[0]);
  TensorPtr cmn_waveform;
  RETURN_IF_NOT_OK(
    Tensor::CreateEmpty(TensorShape({num_channels, num_frames, num_feats}), input->type(), &cmn_waveform));
  RETURN_IF_NOT_OK(ComputeCmnWaveform<T>(input, &cmn_waveform, num_channels, num_frames, num_feats, cmn_window,
                                         min_cmn_window, center, norm_vars));

  std::vector<dsize_t> re_shape = input_shape;
  auto r_it = re_shape.rbegin();
  *r_it++ = num_feats;
  *r_it = num_frames;
  RETURN_IF_NOT_OK(cmn_waveform->Reshape(TensorShape(re_shape)));

  constexpr int specify_input_shape = 2;
  constexpr int specify_first_shape = 1;
  if (input_shape.size() == specify_input_shape && cmn_waveform->shape()[0] == specify_first_shape) {
    cmn_waveform->Squeeze();
  }
  *output = cmn_waveform;
  return Status::OK();
}

Status SlidingWindowCmn(const std::shared_ptr<Tensor> &input, std::shared_ptr<Tensor> *output, int32_t cmn_window,
                        int32_t min_cmn_window, bool center, bool norm_vars) {
  TensorShape input_shape = input->shape();
  CHECK_FAIL_RETURN_UNEXPECTED(input_shape.Size() >= kMinAudioRank,
                               "SlidingWindowCmn: input tensor is not in shape of <..., freq, time>.");

  if (input->type().IsNumeric() && input->type().value() != DataType::DE_FLOAT64) {
    std::shared_ptr<Tensor> temp;
    RETURN_IF_NOT_OK(TypeCast(input, &temp, DataType(DataType::DE_FLOAT32)));
    RETURN_IF_NOT_OK(SlidingWindowCmnHelper<float>(temp, output, cmn_window, min_cmn_window, center, norm_vars));
  } else if (input->type().value() == DataType::DE_FLOAT64) {
    RETURN_IF_NOT_OK(SlidingWindowCmnHelper<double>(input, output, cmn_window, min_cmn_window, center, norm_vars));
  } else {
    RETURN_STATUS_UNEXPECTED("SlidingWindowCmn: input tensor type should be int, float or double, but got: " +
                             input->type().ToString());
  }
  return Status::OK();
}
}  // namespace dataset
}  // namespace mindspore
