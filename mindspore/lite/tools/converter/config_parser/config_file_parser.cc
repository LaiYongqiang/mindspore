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

#include "tools/converter/config_parser/config_file_parser.h"
#include "tools/common/parse_config_utils.h"
#include "include/errorcode.h"
#include "src/common/log_adapter.h"

namespace mindspore {
namespace lite {
namespace {
constexpr auto kCommonQuantParam = "common_quant_param";
constexpr auto kFullQuantParam = "full_quant_param";
constexpr auto kMixedBitWeightQuantParam = "mixed_bit_weight_quant_param";
constexpr auto kDataPreprocessParam = "data_preprocess_param";
constexpr auto kRegistry = "registry";
constexpr auto kAclOptionParam = "acl_option_cfg_param";
}  // namespace
int ConfigFileParser::ParseConfigFile(const std::string &config_file_path) {
  std::map<std::string, std::map<std::string, std::string>> maps;
  auto ret = mindspore::lite::ParseConfigFile(config_file_path, &maps);
  if (ret != RET_OK) {
    MS_LOG(ERROR) << "Parse config file failed.";
    return ret;
  }
  ret = ParseDataPreProcessString(maps);
  if (ret != RET_OK) {
    MS_LOG(ERROR) << "ParseDataPreProcessString failed.";
    return ret;
  }
  ret = ParseCommonQuantString(maps);
  if (ret != RET_OK) {
    MS_LOG(ERROR) << "ParseCommonQuantString failed.";
    return ret;
  }
  ret = ParseMixedBitQuantString(maps);
  if (ret != RET_OK) {
    MS_LOG(ERROR) << "ParseMixedBitQuantString failed.";
    return ret;
  }
  ret = ParseFullQuantString(maps);
  if (ret != RET_OK) {
    MS_LOG(ERROR) << "ParseFullQuantString failed.";
    return ret;
  }
  ret = ParseRegistryInfoString(maps);
  if (ret != RET_OK) {
    MS_LOG(ERROR) << "ParseExtendedintegrationString failed.";
    return ret;
  }
  ret = ParseAclOptionCfgString(maps);
  if (ret != RET_OK) {
    MS_LOG(ERROR) << "ParseAclOptionCfgString failed.";
    return ret;
  }
  return RET_OK;
}

int ConfigFileParser::SetMapData(const std::map<std::string, std::string> &input_map,
                                 const std::map<std::string, std::string &> &parse_map, const std::string &section) {
  for (const auto &map : input_map) {
    if (parse_map.find(map.first) == parse_map.end()) {
      MS_LOG(ERROR) << "INPUT ILLEGAL: `" << map.first << "` is not supported in "
                    << "[" << section << "]";
      return RET_INPUT_PARAM_INVALID;
    } else {
      parse_map.at(map.first) = map.second;
    }
  }
  return RET_OK;
}

int ConfigFileParser::ParseDataPreProcessString(const std::map<std::string, std::map<std::string, std::string>> &maps) {
  if (maps.find(kDataPreprocessParam) != maps.end()) {
    const auto &map = maps.at(kDataPreprocessParam);
    std::map<std::string, std::string &> parse_map{
      {"calibrate_path", data_pre_process_string_.calibrate_path},
      {"calibrate_size", data_pre_process_string_.calibrate_size},
      {"input_type", data_pre_process_string_.input_type},
      {"image_to_format", data_pre_process_string_.image_to_format},
      {"normalize_mean", data_pre_process_string_.normalize_mean},
      {"normalize_std", data_pre_process_string_.normalize_std},
      {"resize_width", data_pre_process_string_.resize_width},
      {"resize_height", data_pre_process_string_.resize_height},
      {"resize_method", data_pre_process_string_.resize_method},
      {"center_crop_width", data_pre_process_string_.center_crop_width},
      {"center_crop_height", data_pre_process_string_.center_crop_height},
    };
    return SetMapData(map, parse_map, kDataPreprocessParam);
  }
  return RET_OK;
}

int ConfigFileParser::ParseCommonQuantString(const std::map<std::string, std::map<std::string, std::string>> &maps) {
  if (maps.find(kCommonQuantParam) != maps.end()) {
    const auto &map = maps.at(kCommonQuantParam);
    std::map<std::string, std::string &> parse_map{
      {"quant_type", common_quant_string_.quant_type},
      {"bit_num", common_quant_string_.bit_num},
      {"min_quant_weight_size", common_quant_string_.min_quant_weight_size},
      {"min_quant_weight_channel", common_quant_string_.min_quant_weight_channel},
      {"skip_node", common_quant_string_.skip_node},
      {"debug_info_save_path", common_quant_string_.debug_info_save_path},
    };
    return SetMapData(map, parse_map, kCommonQuantParam);
  }
  return RET_OK;
}

int ConfigFileParser::ParseMixedBitQuantString(const std::map<std::string, std::map<std::string, std::string>> &maps) {
  if (maps.find(kMixedBitWeightQuantParam) != maps.end()) {
    const auto &map = maps.at(kMixedBitWeightQuantParam);
    std::map<std::string, std::string &> parse_map{
      {"init_scale", mixed_bit_quant_string_.init_scale},
      {"auto_tune", mixed_bit_quant_string_.auto_tune},
    };
    return SetMapData(map, parse_map, kMixedBitWeightQuantParam);
  }
  return RET_OK;
}

int ConfigFileParser::ParseFullQuantString(const std::map<std::string, std::map<std::string, std::string>> &maps) {
  if (maps.find(kFullQuantParam) != maps.end()) {
    const auto &map = maps.at(kFullQuantParam);
    std::map<std::string, std::string &> parse_map{
      {"activation_quant_method", full_quant_string_.activation_quant_method},
      {"bias_correction", full_quant_string_.bias_correction},
    };
    return SetMapData(map, parse_map, kFullQuantParam);
  }
  return RET_OK;
}

int ConfigFileParser::ParseRegistryInfoString(const std::map<std::string, std::map<std::string, std::string>> &maps) {
  if (maps.find(kRegistry) != maps.end()) {
    const auto &map = maps.at(kRegistry);
    std::map<std::string, std::string &> parse_map{
      {"plugin_path", registry_info_string_.plugin_path},
      {"disable_fusion", registry_info_string_.disable_fusion},
    };
    return SetMapData(map, parse_map, kRegistry);
  }
  return RET_OK;
}

int ConfigFileParser::ParseAclOptionCfgString(const std::map<std::string, std::map<std::string, std::string>> &maps) {
  if (maps.find(kAclOptionParam) != maps.end()) {
    const auto &map = maps.at(kAclOptionParam);
    std::map<std::string, std::string &> parse_map{
      {"device_id", acl_option_cfg_string_.device_id},
      {"input_format", acl_option_cfg_string_.input_format},
      {"input_shape_vector", acl_option_cfg_string_.input_shape_vector},
      {"input_shape", acl_option_cfg_string_.input_shape},
      {"output_type", acl_option_cfg_string_.output_type},
      {"precision_mode", acl_option_cfg_string_.precision_mode},
      {"op_select_impl_mode", acl_option_cfg_string_.op_select_impl_mode},
      {"fusion_switch_config_file_path", acl_option_cfg_string_.fusion_switch_config_file_path},
      {"dynamic_batch_size", acl_option_cfg_string_.dynamic_batch_size},
      {"buffer_optimize", acl_option_cfg_string_.buffer_optimize},
      {"insert_op_config_file_path", acl_option_cfg_string_.insert_op_config_file_path}};
    return SetMapData(map, parse_map, kAclOptionParam);
  }
  return RET_OK;
}
}  // namespace lite
}  // namespace mindspore
