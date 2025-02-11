/**
 * Copyright 2019-2021 Huawei Technologies Co., Ltd
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
#include <iostream>
#include <memory>
#include <vector>

#include "common/common.h"
#include "minddata/dataset/core/client.h"
#include "minddata/dataset/core/tensor.h"
#include "minddata/dataset/engine/datasetops/source/image_folder_op.h"
#include "minddata/dataset/engine/datasetops/source/tf_reader_op.h"
#include "minddata/dataset/engine/jagged_connector.h"
#include "minddata/dataset/kernels/image/decode_op.h"
#include "minddata/dataset/kernels/image/resize_op.h"
#include "minddata/dataset/engine/datasetops/source/sampler/sequential_sampler.h"
#include "minddata/dataset/kernels/tensor_op.h"
#include "utils/log_adapter.h"

using namespace mindspore::dataset;
using mindspore::LogStream;
using mindspore::MsLogLevel::INFO;

namespace mindspore {
namespace dataset {
namespace test {
class NoOp : public TensorOp {
 public:
  NoOp(){};

  ~NoOp(){};

  Status Compute(const std::shared_ptr<Tensor> &input, std::shared_ptr<Tensor> *output) override {
    *output = std::move(input);
    return Status::OK();
  };

  void Print(std::ostream &out) const override { out << "NoOp"; };

  std::string Name() const override { return kNoOp; }
};

class ThreeToOneOp : public TensorOp {
 public:
  ThreeToOneOp(){};

  ~ThreeToOneOp(){};

  uint32_t NumInput() override { return 3; }
  // Compute function that holds the actual implementation of the operation.
  Status Compute(const TensorRow &input, TensorRow *output) override {
    output->push_back(input[0]);
    return Status::OK();
  };

  void Print(std::ostream &out) const override { out << "ThreeToOneOp"; };

  std::string Name() const override { return "ThreeToOneOp"; }
};

class OneToThreeOp : public TensorOp {
 public:
  OneToThreeOp(){};

  ~OneToThreeOp(){};

  uint32_t NumOutput() override { return 3; }

  // Compute function that holds the actual implementation of the operation.
  // Simply pushing the same shared pointer of the first element of input vector three times.
  Status Compute(const TensorRow &input, TensorRow *output) override {
    output->push_back(input[0]);
    output->push_back(input[0]);
    output->push_back(input[0]);
    return Status::OK();
  };

  void Print(std::ostream &out) const override { out << "OneToThreeOp"; };

  std::string Name() const override { return "OneToThreeOp"; };
};
}  // namespace test
}  // namespace dataset
}  // namespace mindspore

class MindDataTestMapOp : public UT::DatasetOpTesting {
 public:
  void SetUp() override {
    DatasetOpTesting::SetUp();
    dataset_path_ = datasets_root_path_ + "" + "/testDataset2/testDataset2.data";
    schema_path_ = datasets_root_path_ + "" + "/testDataset2/datasetSchema.json";

    GlobalInit();

    // Start with an empty execution tree
    my_tree_ = std::make_shared<ExecutionTree>();
  }

  std::shared_ptr<TFReaderOp> CreateTFReaderOp() {
    std::shared_ptr<ConfigManager> config_manager = GlobalContext::config_manager();
    auto op_connector_size = config_manager->op_connector_size();

    std::unique_ptr<DataSchema> schema = std::make_unique<DataSchema>();
    std::vector<std::string> columns_to_load = {"image", "label", "A", "B"};
    (void)schema->LoadSchemaFile(schema_path_, columns_to_load);
    std::vector<std::string> files = {dataset_path_};
    std::shared_ptr<TFReaderOp> my_tfreader_op = std::make_shared<TFReaderOp>(
      1, 2, 0, files, std::move(schema), op_connector_size, columns_to_load, false, 1, 0, false);
    (void)my_tfreader_op->Init();
    return my_tfreader_op;
  }

  std::shared_ptr<ExecutionTree> my_tree_;

 private:
  std::string dataset_path_;
  std::string schema_path_;
};

std::shared_ptr<ImageFolderOp> ImageFolder(int64_t num_works, int64_t rows, int64_t conns, std::string path,
                                           bool shuf = false, std::shared_ptr<SamplerRT> sampler = nullptr,
                                           std::map<std::string, int32_t> map = {}, bool decode = false) {
  std::unique_ptr<DataSchema> schema = std::make_unique<DataSchema>();
  TensorShape scalar = TensorShape::CreateScalar();
  (void)schema->AddColumn(ColDescriptor("image", DataType(DataType::DE_UINT8), TensorImpl::kFlexible, 1));
  (void)schema->AddColumn(ColDescriptor("label", DataType(DataType::DE_INT32), TensorImpl::kFlexible, 0, &scalar));
  std::set<std::string> ext = {".jpg", ".JPEG"};
  if (sampler == nullptr) {
    int64_t num_samples = 0;  // default num samples of 0 means to sample entire set of data
    int64_t start_index = 0;
    sampler = std::make_shared<SequentialSamplerRT>(start_index, num_samples);
  }
  std::shared_ptr<ImageFolderOp> so =
    std::make_shared<ImageFolderOp>(num_works, path, conns, false, decode, ext, map, std::move(schema), sampler);
  return so;
}

// TestAsMap scenario:
//    TFReaderOp reads a dataset that have column ordering |image|label|A|B|.
//    A TensorOp that does nothing picks the "image" column and produces a column named "X".
//    Thus, based on the new MapOp behaviour, the column ordering will be |X|label|A|B|.
//    Verify that the "image" column is removed and "X" column is added.
TEST_F(MindDataTestMapOp, TestAsMap) {
  Status rc;
  MS_LOG(INFO) << "Doing TestAsMap.";

  // Note: The above TFReader config yields 5 buffers, each with 2 rows, for a total of 10 rows.
  auto my_tfreader_op = this->CreateTFReaderOp();
  rc = my_tree_->AssociateNode(my_tfreader_op);
  EXPECT_TRUE(rc.IsOk());
  auto my_no_op = std::make_shared<mindspore::dataset::test::NoOp>();
  std::vector<std::shared_ptr<TensorOp>> my_func_list;
  my_func_list.push_back(my_no_op);
  std::shared_ptr<ConfigManager> config_manager = GlobalContext::config_manager();
  auto op_connector_size = config_manager->op_connector_size();
  std::vector<std::string> in_columns = {"image"};
  std::vector<std::string> out_columns = {"X"};
  std::shared_ptr<MapOp> my_map_op =
    std::make_shared<MapOp>(in_columns, out_columns, std::move(my_func_list), 1, op_connector_size);
  rc = my_tree_->AssociateNode(my_map_op);
  EXPECT_TRUE(rc.IsOk());
  rc = my_map_op->AddChild(my_tfreader_op);
  EXPECT_TRUE(rc.IsOk());

  // Assign the tree root
  rc = my_tree_->AssignRoot(my_map_op);
  EXPECT_TRUE(rc.IsOk());

  // Now prepare the tree
  rc = my_tree_->Prepare();
  EXPECT_TRUE(rc.IsOk());
  rc = my_tree_->Launch();
  EXPECT_TRUE(rc.IsOk());

  // Start the loop of reading tensors from our pipeline
  DatasetIterator di(my_tree_);
  TensorMap tensor_map;
  rc = di.GetNextAsMap(&tensor_map);
  EXPECT_TRUE(rc.IsOk());
  EXPECT_EQ(tensor_map.size(), 4);
  EXPECT_EQ(tensor_map.find("image"), tensor_map.end());
  EXPECT_NE(tensor_map.find("label"), tensor_map.end());
  EXPECT_NE(tensor_map.find("X"), tensor_map.end());
  EXPECT_NE(tensor_map.find("A"), tensor_map.end());
  EXPECT_NE(tensor_map.find("B"), tensor_map.end());
}

// Test3to1 scenario:
//    TFReaderOp reads a dataset that have column ordering |image|label|A|B|.
//    A 3-to-1 TensorOp picks the columns [image, A, B] and produce a column named "X".
//    Thus, based on the new MapOp behaviour, the column ordering will be |X|label|.
//    Verify that the only columns "X" and "label" exist.
TEST_F(MindDataTestMapOp, Test3to1) {
  Status rc;
  MS_LOG(INFO) << "Doing Test3to1.";

  // Note: The above TFReader config yields 5 buffers, each with 2 rows, for a total of 10 rows.
  auto my_tfreader_op = this->CreateTFReaderOp();
  rc = my_tree_->AssociateNode(my_tfreader_op);
  EXPECT_TRUE(rc.IsOk());
  auto my_op = std::make_shared<mindspore::dataset::test::ThreeToOneOp>();
  std::vector<std::shared_ptr<TensorOp>> my_func_list;
  my_func_list.push_back(my_op);
  std::shared_ptr<ConfigManager> config_manager = GlobalContext::config_manager();
  auto op_connector_size = config_manager->op_connector_size();
  std::vector<std::string> in_columns = {"image", "A", "B"};
  std::vector<std::string> out_columns = {"X"};

  std::shared_ptr<MapOp> my_map_op =
    std::make_shared<MapOp>(in_columns, out_columns, std::move(my_func_list), 1, op_connector_size);

  rc = my_tree_->AssociateNode(my_map_op);
  EXPECT_TRUE(rc.IsOk());
  rc = my_map_op->AddChild(my_tfreader_op);
  EXPECT_TRUE(rc.IsOk());
  rc = my_tree_->AssignRoot(my_map_op);
  EXPECT_TRUE(rc.IsOk());
  rc = my_tree_->Prepare();
  EXPECT_TRUE(rc.IsOk());
  rc = my_tree_->Launch();
  EXPECT_TRUE(rc.IsOk());

  // Start the loop of reading tensors from our pipeline
  DatasetIterator di(my_tree_);
  TensorMap tensor_map;
  rc = di.GetNextAsMap(&tensor_map);
  EXPECT_TRUE(rc.IsOk());
  while (!tensor_map.empty()) {
    EXPECT_EQ(tensor_map.size(), 2);
    EXPECT_EQ(tensor_map.find("image"), tensor_map.end());
    EXPECT_NE(tensor_map.find("label"), tensor_map.end());
    EXPECT_NE(tensor_map.find("X"), tensor_map.end());
    EXPECT_EQ(tensor_map.find("A"), tensor_map.end());
    EXPECT_EQ(tensor_map.find("B"), tensor_map.end());
    rc = di.GetNextAsMap(&tensor_map);
    EXPECT_TRUE(rc.IsOk());
  }
}

// Test1to3 scenario:
//    TFReaderOp reads a dataset that have column ordering |image|label|A|B|.
//    A 1-to-3 TensorOp picks the columns [image] and produce a column named [X, Y, Z].
//    Thus, based on the new MapOp behaviour, the column ordering will be |X|Y|Z|label|A|B|.
//    Verify that the only columns X, Y, Z are added (to the front) and followed by columns label, A, B..
TEST_F(MindDataTestMapOp, Test1to3) {
  Status rc;
  MS_LOG(INFO) << "Doing Test1to3.";

  // Note: The above TFReader config yields 5 buffers, each with 2 rows, for a total of 10 rows.
  auto my_tfreader_op = this->CreateTFReaderOp();
  rc = my_tree_->AssociateNode(my_tfreader_op);
  EXPECT_TRUE(rc.IsOk());
  auto my_op = std::make_shared<mindspore::dataset::test::OneToThreeOp>();
  std::vector<std::shared_ptr<TensorOp>> my_func_list;
  my_func_list.push_back(my_op);
  std::shared_ptr<ConfigManager> config_manager = GlobalContext::config_manager();
  auto op_connector_size = config_manager->op_connector_size();
  std::vector<std::string> in_columns = {"image"};
  std::vector<std::string> out_columns = {"X", "Y", "Z"};

  std::shared_ptr<MapOp> my_map_op =
    std::make_shared<MapOp>(in_columns, out_columns, std::move(my_func_list), 1, op_connector_size);
  // ProjectOp
  std::vector<std::string> columns_to_project = {"X", "Y", "Z", "label", "A", "B"};
  std::shared_ptr<ProjectOp> my_project_op = std::make_shared<ProjectOp>(columns_to_project);
  rc = my_tree_->AssociateNode(my_project_op);
  ASSERT_TRUE(rc.IsOk());

  rc = my_tree_->AssignRoot(my_project_op);
  ASSERT_TRUE(rc.IsOk());

  rc = my_tree_->AssociateNode(my_map_op);
  EXPECT_TRUE(rc.IsOk());

  rc = my_project_op->AddChild(my_map_op);
  EXPECT_TRUE(rc.IsOk());

  rc = my_map_op->AddChild(my_tfreader_op);
  EXPECT_TRUE(rc.IsOk());
  rc = my_tree_->Prepare();
  EXPECT_TRUE(rc.IsOk());
  rc = my_tree_->Launch();
  EXPECT_TRUE(rc.IsOk());

  // Start the loop of reading tensors from our pipeline
  DatasetIterator di(my_tree_);
  TensorMap tensor_map;
  rc = di.GetNextAsMap(&tensor_map);
  EXPECT_TRUE(rc.IsOk());
  EXPECT_EQ(tensor_map.size(), 6);
  EXPECT_EQ(tensor_map.find("image"), tensor_map.end());
  EXPECT_NE(tensor_map.find("label"), tensor_map.end());
  EXPECT_NE(tensor_map.find("A"), tensor_map.end());
  EXPECT_NE(tensor_map.find("B"), tensor_map.end());
  EXPECT_NE(tensor_map.find("X"), tensor_map.end());
  EXPECT_NE(tensor_map.find("Y"), tensor_map.end());
  EXPECT_NE(tensor_map.find("Z"), tensor_map.end());

  // Getting the next row as vector (by position).
  TensorRow tensor_list;
  rc = di.FetchNextTensorRow(&tensor_list);
  EXPECT_TRUE(rc.IsOk());

  // Based on the schema file, create the golden result to compare with.
  std::vector<DataType::Type> golden_types({DataType::Type::DE_UINT8, DataType::Type::DE_UINT8,
                                            DataType::Type::DE_UINT8, DataType::Type::DE_INT64,
                                            DataType::Type::DE_FLOAT32, DataType::Type::DE_INT64});

  std::vector<uint64_t> golden_ranks({3, 3, 3, 1, 4, 1});

  std::vector<TensorShape> golden_shapes({TensorShape({3, 4, 2}), TensorShape({3, 4, 2}), TensorShape({3, 4, 2}),
                                          TensorShape({7}), TensorShape({1, 13, 14, 12}), TensorShape({9})});

  while (!tensor_list.empty()) {
    for (uint32_t i = 0; i < tensor_list.size(); i++) {
      EXPECT_EQ(tensor_list[i]->type(), golden_types[i]);
      EXPECT_EQ(tensor_list[i]->Rank(), golden_ranks[i]);
      EXPECT_EQ(tensor_list[i]->shape(), golden_shapes[i]);
      EXPECT_NE(tensor_list[i]->GetBuffer(), nullptr);
    }
    rc = di.FetchNextTensorRow(&tensor_list);
    EXPECT_TRUE(rc.IsOk());
  }
}

// TestMultiTensorOp scenario:
//    TFReaderOp reads a dataset that have column ordering |image|label|A|B|.
//    A series of 3-to-1 and 1-to-3 TensorOps are applied to [image, A, B] and
//    produce final output columns [X, Y, Z].
//    Based on the new MapOp behaviour, the column ordering will be |X|Y|Z|label|.
TEST_F(MindDataTestMapOp, TestMultiTensorOp) {
  Status rc;
  MS_LOG(INFO) << "Doing TestMultiTensorOp.";

  // Note: The above TFReader config yields 5 buffers, each with 2 rows, for a total of 10 rows.
  auto my_tfreader_op = this->CreateTFReaderOp();
  rc = my_tree_->AssociateNode(my_tfreader_op);
  EXPECT_TRUE(rc.IsOk());
  auto my_op1 = std::make_shared<mindspore::dataset::test::ThreeToOneOp>();
  auto my_op2 = std::make_shared<mindspore::dataset::test::OneToThreeOp>();
  std::vector<std::shared_ptr<TensorOp>> my_func_list;
  my_func_list.push_back(my_op1);
  my_func_list.push_back(my_op2);
  std::shared_ptr<ConfigManager> config_manager = GlobalContext::config_manager();
  auto op_connector_size = config_manager->op_connector_size();
  std::vector<std::string> in_columns = {"image", "A", "B"};
  std::vector<std::string> out_columns = {"X", "Y", "Z"};

  std::shared_ptr<MapOp> my_map_op =
    std::make_shared<MapOp>(in_columns, out_columns, std::move(my_func_list), 1, op_connector_size);

  rc = my_tree_->AssociateNode(my_map_op);
  EXPECT_TRUE(rc.IsOk());
  rc = my_map_op->AddChild(my_tfreader_op);
  EXPECT_TRUE(rc.IsOk());
  rc = my_tree_->AssignRoot(my_map_op);
  EXPECT_TRUE(rc.IsOk());
  rc = my_tree_->Prepare();
  EXPECT_TRUE(rc.IsOk());
  rc = my_tree_->Launch();
  EXPECT_TRUE(rc.IsOk());

  // Start the loop of reading tensors from our pipeline
  DatasetIterator di(my_tree_);
  TensorMap tensor_map;
  rc = di.GetNextAsMap(&tensor_map);
  EXPECT_TRUE(rc.IsOk());
  while (!tensor_map.empty()) {
    EXPECT_EQ(tensor_map.size(), 4);
    EXPECT_EQ(tensor_map.find("image"), tensor_map.end());
    EXPECT_EQ(tensor_map.find("A"), tensor_map.end());
    EXPECT_EQ(tensor_map.find("B"), tensor_map.end());
    EXPECT_NE(tensor_map.find("label"), tensor_map.end());
    EXPECT_NE(tensor_map.find("X"), tensor_map.end());
    EXPECT_NE(tensor_map.find("Y"), tensor_map.end());
    EXPECT_NE(tensor_map.find("Z"), tensor_map.end());

    // XYZ are Tensor shared_ptr to image, so it should have the same shape as image column.
    EXPECT_EQ(tensor_map["X"]->shape(), TensorShape({3, 4, 2}));
    EXPECT_EQ(tensor_map["Y"]->shape(), TensorShape({3, 4, 2}));
    EXPECT_EQ(tensor_map["Z"]->shape(), TensorShape({3, 4, 2}));
    rc = di.GetNextAsMap(&tensor_map);
    EXPECT_TRUE(rc.IsOk());
  }
}

TEST_F(MindDataTestMapOp, TestTFReaderRepeatMap) {
  Status rc;
  MS_LOG(INFO) << "Doing TestTFReaderRepeatMap.";
  uint32_t num_repeats = 3;

  // Note: The above TFReader config yields 5 buffers, each with 2 rows, for a total
  // of 10 rows.
  auto my_tfreader_op = this->CreateTFReaderOp();
  rc = my_tree_->AssociateNode(my_tfreader_op);
  EXPECT_TRUE(rc.IsOk());
  auto my_no_op = std::make_shared<mindspore::dataset::test::NoOp>();
  std::vector<std::shared_ptr<TensorOp>> my_func_list;
  my_func_list.push_back(my_no_op);

  std::shared_ptr<RepeatOp> my_repeat_op = std::make_shared<RepeatOp>(num_repeats);
  rc = my_tree_->AssociateNode(my_repeat_op);
  EXPECT_TRUE(rc.IsOk());

  std::shared_ptr<ConfigManager> config_manager = GlobalContext::config_manager();
  auto op_connector_size = config_manager->op_connector_size();
  std::vector<std::string> in_columns = {"label"};
  std::vector<std::string> out_columns = {};

  std::shared_ptr<MapOp> my_map_op =
    std::make_shared<MapOp>(in_columns, out_columns, std::move(my_func_list), 5, op_connector_size);

  rc = my_tree_->AssociateNode(my_map_op);
  EXPECT_TRUE(rc.IsOk());

  rc = my_map_op->AddChild(my_repeat_op);
  EXPECT_TRUE(rc.IsOk());

  my_tfreader_op->SetTotalRepeats(num_repeats);
  my_tfreader_op->SetNumRepeatsPerEpoch(num_repeats);
  rc = my_repeat_op->AddChild(my_tfreader_op);
  EXPECT_TRUE(rc.IsOk());

  rc = my_tree_->AssignRoot(my_map_op);
  EXPECT_TRUE(rc.IsOk());

  rc = my_tree_->Prepare();
  EXPECT_TRUE(rc.IsOk());
  rc = my_tree_->Launch();
  EXPECT_TRUE(rc.IsOk());

  // Start the loop of reading tensors from our pipeline
  DatasetIterator di(my_tree_);
  TensorRow tensor_list;
  rc = di.FetchNextTensorRow(&tensor_list);
  EXPECT_TRUE(rc.IsOk());
  EXPECT_EQ(tensor_list.size(), 4);
  uint32_t row_count = 0;
  while (!tensor_list.empty()) {
    row_count++;
    MS_LOG(INFO) << "row_count: " << row_count << ".";
    rc = di.FetchNextTensorRow(&tensor_list);
    EXPECT_TRUE(rc.IsOk());
  }
  ASSERT_EQ(row_count, 10 * num_repeats);
}

TEST_F(MindDataTestMapOp, TestTFReaderMapRepeat) {
  Status rc;
  MS_LOG(INFO) << "Doing TestTFReaderMapRepeat.";
  uint32_t num_repeats = 3;

  // Note: The above TFReader config yields 5 buffers, each with 2 rows, for a total
  // of 10 rows.
  auto my_tfreader_op = this->CreateTFReaderOp();
  rc = my_tree_->AssociateNode(my_tfreader_op);
  EXPECT_TRUE(rc.IsOk());
  auto my_no_op = std::make_shared<mindspore::dataset::test::NoOp>();
  std::vector<std::shared_ptr<TensorOp>> my_func_list;
  my_func_list.push_back(my_no_op);

  std::shared_ptr<RepeatOp> my_repeat_op = std::make_shared<RepeatOp>(num_repeats);
  rc = my_tree_->AssociateNode(my_repeat_op);
  EXPECT_TRUE(rc.IsOk());

  std::shared_ptr<ConfigManager> config_manager = GlobalContext::config_manager();
  auto op_connector_size = config_manager->op_connector_size();
  std::vector<std::string> input_columns = {"label"};
  std::vector<std::string> output_columns = {};
  std::shared_ptr<MapOp> my_map_op =
    std::make_shared<MapOp>(input_columns, output_columns, std::move(my_func_list), 50, op_connector_size);

  rc = my_tree_->AssociateNode(my_map_op);
  EXPECT_TRUE(rc.IsOk());

  my_map_op->SetTotalRepeats(num_repeats);
  my_map_op->SetNumRepeatsPerEpoch(num_repeats);
  rc = my_repeat_op->AddChild(my_map_op);
  EXPECT_TRUE(rc.IsOk());

  my_tfreader_op->SetTotalRepeats(num_repeats);
  my_tfreader_op->SetNumRepeatsPerEpoch(num_repeats);
  rc = my_map_op->AddChild(my_tfreader_op);
  EXPECT_TRUE(rc.IsOk());

  rc = my_tree_->AssignRoot(my_repeat_op);
  EXPECT_TRUE(rc.IsOk());

  rc = my_tree_->Prepare();
  EXPECT_TRUE(rc.IsOk());
  rc = my_tree_->Launch();
  EXPECT_TRUE(rc.IsOk());

  // Start the loop of reading tensors from our pipeline
  DatasetIterator di(my_tree_);
  TensorRow tensor_list;
  rc = di.FetchNextTensorRow(&tensor_list);
  EXPECT_TRUE(rc.IsOk());
  EXPECT_EQ(tensor_list.size(), 4);
  uint32_t row_count = 0;
  while (!tensor_list.empty()) {
    row_count++;
    MS_LOG(INFO) << "row_count: " << row_count << ".";
    rc = di.FetchNextTensorRow(&tensor_list);
    EXPECT_TRUE(rc.IsOk());
  }
  ASSERT_EQ(row_count, 10 * num_repeats);
}

TEST_F(MindDataTestMapOp, TFReader_Decode_Repeat_Resize) {
  Status rc;
  MS_LOG(INFO) << "Doing TFReader_Decode_Repeat_Resize.";
  uint32_t num_repeats = 2;

  std::string dataset_path = datasets_root_path_ + "/" + "test_tf_file_3_images/train-0000-of-0001.data";
  std::shared_ptr<ConfigManager> config_manager = GlobalContext::config_manager();
  auto op_connector_size = config_manager->op_connector_size();
  std::unique_ptr<DataSchema> schema = std::make_unique<DataSchema>();
  std::vector<std::string> columns_to_load = {"image", "label"};
  std::vector<std::string> files = {dataset_path};
  std::shared_ptr<TFReaderOp> my_tfreader_op = std::make_shared<TFReaderOp>(
    1, 2, 0, files, std::move(schema), op_connector_size, columns_to_load, false, 1, 0, false);
  (void)my_tfreader_op->Init();

  rc = my_tree_->AssociateNode(my_tfreader_op);
  EXPECT_TRUE(rc.IsOk());
  auto decode_op = std::make_shared<DecodeOp>();
  std::vector<std::shared_ptr<TensorOp>> my_func_list;
  my_func_list.push_back(decode_op);

  std::shared_ptr<RepeatOp> my_repeat_op = std::make_shared<RepeatOp>(num_repeats);
  rc = my_tree_->AssociateNode(my_repeat_op);
  EXPECT_TRUE(rc.IsOk());
  std::vector<std::string> input_columns = {"image"};
  std::vector<std::string> output_columns = {};
  std::shared_ptr<MapOp> my_map_decode_op =
    std::make_shared<MapOp>(input_columns, output_columns, std::move(my_func_list), 4, op_connector_size);
  rc = my_tree_->AssociateNode(my_map_decode_op);
  EXPECT_TRUE(rc.IsOk());

  auto resize_op = std::make_shared<ResizeOp>(300, 300);
  std::vector<std::shared_ptr<TensorOp>> my_func_list2;
  my_func_list2.push_back(resize_op);
  std::shared_ptr<MapOp> my_map_resize_op =
    std::make_shared<MapOp>(input_columns, output_columns, std::move(my_func_list2), 5, op_connector_size);
  rc = my_tree_->AssociateNode(my_map_resize_op);
  EXPECT_TRUE(rc.IsOk());

  my_tfreader_op->SetTotalRepeats(num_repeats);
  my_tfreader_op->SetNumRepeatsPerEpoch(num_repeats);
  rc = my_map_decode_op->AddChild(my_tfreader_op);
  EXPECT_TRUE(rc.IsOk());

  my_map_decode_op->SetTotalRepeats(num_repeats);
  my_map_decode_op->SetNumRepeatsPerEpoch(num_repeats);
  rc = my_repeat_op->AddChild(my_map_decode_op);
  EXPECT_TRUE(rc.IsOk());

  rc = my_map_resize_op->AddChild(my_repeat_op);
  EXPECT_TRUE(rc.IsOk());

  rc = my_tree_->AssignRoot(my_map_resize_op);
  EXPECT_TRUE(rc.IsOk());

  rc = my_tree_->Prepare();
  EXPECT_TRUE(rc.IsOk());
  rc = my_tree_->Launch();
  EXPECT_TRUE(rc.IsOk());

  // Start the loop of reading tensors from our pipeline
  DatasetIterator di(my_tree_);
  TensorRow tensor_list;
  rc = di.FetchNextTensorRow(&tensor_list);
  EXPECT_TRUE(rc.IsOk());
  EXPECT_EQ(tensor_list.size(), 2);
  uint32_t row_count = 0;
  while (!tensor_list.empty()) {
    row_count++;
    rc = di.FetchNextTensorRow(&tensor_list);
    EXPECT_TRUE(rc.IsOk());
  }

  ASSERT_EQ(row_count, 6);
}

TEST_F(MindDataTestMapOp, ImageFolder_Decode_Repeat_Resize) {
  Status rc;
  MS_LOG(INFO) << "Doing ImageFolder_Decode_Repeat_Resize.";

  std::string folder_path = datasets_root_path_ + "/testPK/data";

  uint32_t num_repeats = 2;
  std::shared_ptr<RepeatOp> repeat_op = std::make_shared<RepeatOp>(num_repeats);
  EXPECT_TRUE(rc.IsOk());

  auto decode_op = std::make_shared<DecodeOp>();
  std::vector<std::shared_ptr<TensorOp>> func_list;
  func_list.push_back(decode_op);
  std::shared_ptr<ConfigManager> config_manager = GlobalContext::config_manager();
  int32_t op_connector_size = config_manager->op_connector_size();
  int32_t num_parallel_workers = config_manager->num_parallel_workers();
  std::vector<std::string> input_columns = {"image"};
  std::vector<std::string> output_columns = {};
  std::shared_ptr<MapOp> map_decode_map =
    std::make_shared<MapOp>(input_columns, output_columns, func_list, 4, op_connector_size);

  auto resize_op = std::make_shared<ResizeOp>(300, 300);
  std::vector<std::shared_ptr<TensorOp>> func_list2;
  func_list2.push_back(resize_op);
  std::shared_ptr<MapOp> map_resize_op =
    std::make_shared<MapOp>(input_columns, output_columns, func_list2, 5, op_connector_size);

  auto image_folder_op = ImageFolder(num_parallel_workers, 2, 32, folder_path, false);
  image_folder_op->SetTotalRepeats(num_repeats);
  image_folder_op->SetNumRepeatsPerEpoch(num_repeats);
  map_decode_map->SetTotalRepeats(num_repeats);
  map_decode_map->SetNumRepeatsPerEpoch(num_repeats);
  my_tree_ = Build({image_folder_op, map_decode_map, repeat_op, map_resize_op});
  rc = my_tree_->Prepare();
  EXPECT_TRUE(rc.IsOk());
  rc = my_tree_->Launch();
  EXPECT_TRUE(rc.IsOk());

  // Start the loop of reading tensors from our pipeline
  DatasetIterator di(my_tree_);
  TensorMap tensor_map;
  ASSERT_OK(di.GetNextAsMap(&tensor_map));
  EXPECT_TRUE(rc.IsOk());
  uint64_t i = 0;
  int32_t label = 0;
  int32_t img_class[] = {0, 1, 2, 3};
  std::string result;
  while (tensor_map.size() != 0) {
    tensor_map["label"]->GetItemAt<int32_t>(&label, {});
    MS_LOG(DEBUG) << "row:" << i << "\tlabel:" << label << "\n";
    EXPECT_TRUE(img_class[(i % 44) / 11] == label);
    // Dump all the image into string, to be used as a comparison later.
    result.append((char *)tensor_map["image"]->GetBuffer(), (int64_t)tensor_map["image"]->Size());
    ASSERT_OK(di.GetNextAsMap(&tensor_map));
    i++;
  }
  EXPECT_TRUE(i == 88);

  // Part-2 : creating mapop with performance mode = false, to check if the result is the same
  // as when performance mode = true.
  repeat_op = std::make_shared<RepeatOp>(num_repeats);
  EXPECT_TRUE(rc.IsOk());
  map_decode_map = std::make_shared<MapOp>(input_columns, output_columns, func_list, 14, op_connector_size);

  map_resize_op = std::make_shared<MapOp>(input_columns, output_columns, func_list2, 15, op_connector_size);

  image_folder_op = ImageFolder(16, 2, 32, folder_path, false);
  image_folder_op->SetTotalRepeats(num_repeats);
  image_folder_op->SetNumRepeatsPerEpoch(num_repeats);
  map_decode_map->SetTotalRepeats(num_repeats);
  map_decode_map->SetNumRepeatsPerEpoch(num_repeats);
  auto my_tree_2 = Build({image_folder_op, map_decode_map, repeat_op, map_resize_op});

  rc = my_tree_2->Prepare();
  EXPECT_TRUE(rc.IsOk());
  rc = my_tree_2->Launch();
  EXPECT_TRUE(rc.IsOk());

  // Start the loop of reading tensors from our pipeline
  DatasetIterator di2(my_tree_2);
  ASSERT_OK(di2.GetNextAsMap(&tensor_map));
  EXPECT_TRUE(rc.IsOk());
  i = 0;
  label = 0;
  std::string result2;
  while (tensor_map.size() != 0) {
    tensor_map["label"]->GetItemAt<int32_t>(&label, {});
    MS_LOG(DEBUG) << "row:" << i << "\tlabel:" << label << "\n";
    EXPECT_TRUE(img_class[(i % 44) / 11] == label);
    result2.append((char *)tensor_map["image"]->GetBuffer(), (int64_t)tensor_map["image"]->Size());
    ASSERT_OK(di2.GetNextAsMap(&tensor_map));
    i++;
  }
  EXPECT_TRUE(i == 88);

  EXPECT_EQ(result.size(), result2.size());
  EXPECT_EQ(result, result2);
}

TEST_F(MindDataTestMapOp, ImageFolder_Decode_Repeat_Resize_NoInputColumns) {
  Status rc;
  MS_LOG(INFO) << "Doing ImageFolder_Decode_Repeat_Resize_NoInputColumns.";

  std::string folder_path = datasets_root_path_ + "/testPK/data";

  uint32_t num_repeats = 2;
  std::shared_ptr<RepeatOp> repeat_op = std::make_shared<RepeatOp>(num_repeats);
  ;

  auto decode_op = std::make_shared<DecodeOp>();
  std::vector<std::shared_ptr<TensorOp>> func_list;
  func_list.push_back(decode_op);
  std::shared_ptr<ConfigManager> config_manager = GlobalContext::config_manager();
  auto op_connector_size = config_manager->op_connector_size();
  std::vector<std::string> input_columns = {};
  std::vector<std::string> output_columns = {};
  std::shared_ptr<MapOp> map_decode_map =
    std::make_shared<MapOp>(input_columns, output_columns, std::move(func_list), 4, op_connector_size);
  ;

  auto resize_op = std::make_shared<ResizeOp>(300, 300);
  std::vector<std::shared_ptr<TensorOp>> func_list2;
  func_list2.push_back(resize_op);
  std::shared_ptr<MapOp> map_resize_op =
    std::make_shared<MapOp>(input_columns, output_columns, std::move(func_list2), 5, op_connector_size);
  ;

  auto image_folder_op = ImageFolder(16, 2, 32, folder_path, false);
  image_folder_op->SetTotalRepeats(num_repeats);
  image_folder_op->SetNumRepeatsPerEpoch(num_repeats);
  map_decode_map->SetTotalRepeats(num_repeats);
  map_decode_map->SetNumRepeatsPerEpoch(num_repeats);
  my_tree_ = Build({image_folder_op, map_decode_map, repeat_op, map_resize_op});
  rc = my_tree_->Prepare();
  EXPECT_TRUE(rc.IsOk());
  rc = my_tree_->Launch();
  EXPECT_TRUE(rc.IsOk());

  // Start the loop of reading tensors from our pipeline
  DatasetIterator di(my_tree_);
  TensorMap tensor_map;
  ASSERT_OK(di.GetNextAsMap(&tensor_map));
  EXPECT_TRUE(rc.IsOk());
  uint64_t i = 0;
  int32_t label = 0;
  int32_t img_class[] = {0, 1, 2, 3};
  std::string result;
  while (tensor_map.size() != 0) {
    tensor_map["label"]->GetItemAt<int32_t>(&label, {});
    EXPECT_TRUE(img_class[(i % 44) / 11] == label);
    ASSERT_OK(di.GetNextAsMap(&tensor_map));
    i++;
  }
  EXPECT_TRUE(i == 88);
}
