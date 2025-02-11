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

#include "minddata/dataset/core/client.h"
#include "minddata/dataset/engine/data_schema.h"
#include "minddata/dataset/engine/jagged_connector.h"
#include "common/common.h"
#include "gtest/gtest.h"
#include "utils/log_adapter.h"

namespace common = mindspore::common;

using namespace mindspore::dataset;
using mindspore::LogStream;
using mindspore::ExceptionType::NoExceptionType;
using mindspore::MsLogLevel::INFO;

class MindDataTestTFReaderOp : public UT::DatasetOpTesting {};

TEST_F(MindDataTestTFReaderOp, TestTFReaderBasic1) {
  // Start with an empty execution tree
  auto my_tree = std::make_shared<ExecutionTree>();
  Status rc;
  std::string dataset_path;
  dataset_path = datasets_root_path_ + "/testTFTestAllTypes/test.data";

  std::shared_ptr<ConfigManager> config_manager = GlobalContext::config_manager();
  int32_t op_connector_size = config_manager->op_connector_size();
  int32_t num_workers = 1;
  int32_t worker_connector_size = config_manager->worker_connector_size();
  std::vector<std::string> files = {dataset_path};
  std::vector<std::string> columns_to_load = {};

  std::unique_ptr<DataSchema> schema = std::make_unique<DataSchema>();
  schema->LoadSchemaFile(datasets_root_path_ + "/testTFTestAllTypes/datasetSchema.json", {});
  std::shared_ptr<TFReaderOp> my_tfreader_op =
    std::make_shared<TFReaderOp>(num_workers, worker_connector_size, 0, files, std::move(schema), op_connector_size,
                                 columns_to_load, false, 1, 0, false);
  rc = my_tfreader_op->Init();
  ASSERT_TRUE(rc.IsOk());
  rc = my_tree->AssociateNode(my_tfreader_op);
  ASSERT_TRUE(rc.IsOk());

  rc = my_tree->AssignRoot(my_tfreader_op);
  ASSERT_TRUE(rc.IsOk());

  MS_LOG(INFO) << "Launching tree and begin iteration.";
  rc = my_tree->Prepare();
  ASSERT_TRUE(rc.IsOk());

  rc = my_tree->Launch();
  ASSERT_TRUE(rc.IsOk());

  // Start the loop of reading tensors from our pipeline
  DatasetIterator di(my_tree);
  TensorRow tensor_list;
  rc = di.FetchNextTensorRow(&tensor_list);
  ASSERT_TRUE(rc.IsOk());

  int row_count = 0;
  while (!tensor_list.empty()) {
    // Display the tensor by calling the printer on it
    for (int i = 0; i < tensor_list.size(); i++) {
      std::ostringstream ss;
      ss << "(" << tensor_list[i] << "): " << *tensor_list[i] << std::endl;
      MS_LOG(INFO) << "Tensor print: " << ss.str() << ".";
    }

    rc = di.FetchNextTensorRow(&tensor_list);
    ASSERT_TRUE(rc.IsOk());
    row_count++;
  }

  ASSERT_EQ(row_count, 12);
}

TEST_F(MindDataTestTFReaderOp, TestTFReaderLargeRowsPerBuffer) {
  // Start with an empty execution tree
  auto my_tree = std::make_shared<ExecutionTree>();
  Status rc;
  std::string dataset_path;
  dataset_path = datasets_root_path_ + "/testTFTestAllTypes/test.data";

  std::unique_ptr<DataSchema> schema = std::make_unique<DataSchema>();
  schema->LoadSchemaFile(datasets_root_path_ + "/testTFTestAllTypes/datasetSchema.json", {});
  std::shared_ptr<ConfigManager> config_manager = GlobalContext::config_manager();
  int32_t op_connector_size = config_manager->op_connector_size();
  int32_t num_workers = 1;
  int32_t worker_connector_size = config_manager->worker_connector_size();
  std::vector<std::string> files = {dataset_path};
  std::vector<std::string> columns_to_load = {};

  std::shared_ptr<TFReaderOp> my_tfreader_op =
    std::make_shared<TFReaderOp>(num_workers, worker_connector_size, 0, files, std::move(schema), op_connector_size,
                                 columns_to_load, false, 1, 0, false);
  rc = my_tfreader_op->Init();
  ASSERT_TRUE(rc.IsOk());
  rc = my_tree->AssociateNode(my_tfreader_op);
  ASSERT_TRUE(rc.IsOk());

  rc = my_tree->AssignRoot(my_tfreader_op);
  ASSERT_TRUE(rc.IsOk());

  MS_LOG(INFO) << "Launching tree and begin iteration.";
  rc = my_tree->Prepare();
  ASSERT_TRUE(rc.IsOk());

  rc = my_tree->Launch();
  ASSERT_TRUE(rc.IsOk());

  // Start the loop of reading tensors from our pipeline
  DatasetIterator di(my_tree);
  TensorRow tensor_list;
  rc = di.FetchNextTensorRow(&tensor_list);
  ASSERT_TRUE(rc.IsOk());

  int row_count = 0;
  while (!tensor_list.empty()) {
    // Display the tensor by calling the printer on it
    for (int i = 0; i < tensor_list.size(); i++) {
      std::ostringstream ss;
      ss << "(" << tensor_list[i] << "): " << *tensor_list[i] << std::endl;
      MS_LOG(INFO) << "Tensor print: " << ss.str() << ".";
    }

    rc = di.FetchNextTensorRow(&tensor_list);
    ASSERT_TRUE(rc.IsOk());
    row_count++;
  }

  ASSERT_EQ(row_count, 12);
}

TEST_F(MindDataTestTFReaderOp, TestTFReaderSmallRowsPerBuffer) {
  // Start with an empty execution tree
  auto my_tree = std::make_shared<ExecutionTree>();
  Status rc;
  std::string dataset_path;
  dataset_path = datasets_root_path_ + "/testTFTestAllTypes/test.data";

  std::shared_ptr<ConfigManager> config_manager = GlobalContext::config_manager();
  int32_t op_connector_size = config_manager->op_connector_size();
  int32_t num_workers = 1;
  int32_t worker_connector_size = config_manager->worker_connector_size();
  std::vector<std::string> files = {dataset_path};
  std::vector<std::string> columns_to_load = {};

  std::unique_ptr<DataSchema> schema = std::make_unique<DataSchema>();
  schema->LoadSchemaFile(datasets_root_path_ + "/testTFTestAllTypes/datasetSchema.json", {});
  std::shared_ptr<TFReaderOp> my_tfreader_op =
    std::make_shared<TFReaderOp>(num_workers, worker_connector_size, 0, files, std::move(schema), op_connector_size,
                                 columns_to_load, false, 1, 0, false);
  rc = my_tfreader_op->Init();
  ASSERT_TRUE(rc.IsOk());
  rc = my_tree->AssociateNode(my_tfreader_op);
  ASSERT_TRUE(rc.IsOk());

  rc = my_tree->AssignRoot(my_tfreader_op);
  ASSERT_TRUE(rc.IsOk());

  MS_LOG(INFO) << "Launching tree and begin iteration.";
  rc = my_tree->Prepare();
  ASSERT_TRUE(rc.IsOk());

  rc = my_tree->Launch();
  ASSERT_TRUE(rc.IsOk());

  // Start the loop of reading tensors from our pipeline
  DatasetIterator di(my_tree);
  TensorRow tensor_list;
  rc = di.FetchNextTensorRow(&tensor_list);
  ASSERT_TRUE(rc.IsOk());

  int row_count = 0;
  while (!tensor_list.empty()) {
    // Display the tensor by calling the printer on it
    for (int i = 0; i < tensor_list.size(); i++) {
      std::ostringstream ss;
      ss << "(" << tensor_list[i] << "): " << *tensor_list[i] << std::endl;
      MS_LOG(INFO) << "Tensor print: " << ss.str() << ".";
    }

    rc = di.FetchNextTensorRow(&tensor_list);
    ASSERT_TRUE(rc.IsOk());
    row_count++;
  }

  ASSERT_EQ(row_count, 12);
}

TEST_F(MindDataTestTFReaderOp, TestTFReaderLargeQueueSize) {
  // Start with an empty execution tree
  auto my_tree = std::make_shared<ExecutionTree>();
  Status rc;
  std::string dataset_path;
  dataset_path = datasets_root_path_ + "/testTFTestAllTypes/test.data";

  std::shared_ptr<ConfigManager> config_manager = GlobalContext::config_manager();
  int32_t op_connector_size = config_manager->op_connector_size();
  int32_t num_workers = 1;
  int32_t worker_connector_size = 1;
  std::vector<std::string> files = {dataset_path};
  std::vector<std::string> columns_to_load = {};

  std::unique_ptr<DataSchema> schema = std::make_unique<DataSchema>();
  schema->LoadSchemaFile(datasets_root_path_ + "/testTFTestAllTypes/datasetSchema.json", {});
  std::shared_ptr<TFReaderOp> my_tfreader_op =
    std::make_shared<TFReaderOp>(num_workers, worker_connector_size, 0, files, std::move(schema), op_connector_size,
                                 columns_to_load, false, 1, 0, false);
  rc = my_tfreader_op->Init();
  ASSERT_TRUE(rc.IsOk());
  rc = my_tree->AssociateNode(my_tfreader_op);
  ASSERT_TRUE(rc.IsOk());

  rc = my_tree->AssignRoot(my_tfreader_op);
  ASSERT_TRUE(rc.IsOk());

  MS_LOG(INFO) << "Launching tree and begin iteration.";
  rc = my_tree->Prepare();
  ASSERT_TRUE(rc.IsOk());

  rc = my_tree->Launch();
  ASSERT_TRUE(rc.IsOk());

  // Start the loop of reading tensors from our pipeline
  DatasetIterator di(my_tree);
  TensorRow tensor_list;
  rc = di.FetchNextTensorRow(&tensor_list);
  ASSERT_TRUE(rc.IsOk());

  int row_count = 0;
  while (!tensor_list.empty()) {
    // Display the tensor by calling the printer on it
    for (int i = 0; i < tensor_list.size(); i++) {
      std::ostringstream ss;
      ss << "(" << tensor_list[i] << "): " << *tensor_list[i] << std::endl;
      MS_LOG(INFO) << "Tensor print: " << ss.str() << ".";
    }

    rc = di.FetchNextTensorRow(&tensor_list);
    ASSERT_TRUE(rc.IsOk());
    row_count++;
  }

  ASSERT_EQ(row_count, 12);
}

TEST_F(MindDataTestTFReaderOp, TestTFReaderOneThread) {
  // Start with an empty execution tree
  auto my_tree = std::make_shared<ExecutionTree>();
  Status rc;
  std::string dataset_path;
  dataset_path = datasets_root_path_ + "/testTFTestAllTypes/test.data";

  std::shared_ptr<ConfigManager> config_manager = GlobalContext::config_manager();
  int32_t op_connector_size = config_manager->op_connector_size();
  int32_t num_workers = 1;
  int32_t worker_connector_size = config_manager->worker_connector_size();
  std::vector<std::string> files = {dataset_path};
  std::vector<std::string> columns_to_load = {};

  std::unique_ptr<DataSchema> schema = std::make_unique<DataSchema>();
  schema->LoadSchemaFile(datasets_root_path_ + "/testTFTestAllTypes/datasetSchema.json", {});
  std::shared_ptr<TFReaderOp> my_tfreader_op =
    std::make_shared<TFReaderOp>(num_workers, worker_connector_size, 0, files, std::move(schema), op_connector_size,
                                 columns_to_load, false, 1, 0, false);
  rc = my_tfreader_op->Init();
  ASSERT_TRUE(rc.IsOk());
  rc = my_tree->AssociateNode(my_tfreader_op);
  ASSERT_TRUE(rc.IsOk());

  rc = my_tree->AssignRoot(my_tfreader_op);
  ASSERT_TRUE(rc.IsOk());

  MS_LOG(INFO) << "Launching tree and begin iteration.";
  rc = my_tree->Prepare();
  ASSERT_TRUE(rc.IsOk());

  rc = my_tree->Launch();
  ASSERT_TRUE(rc.IsOk());

  // Start the loop of reading tensors from our pipeline
  DatasetIterator di(my_tree);
  TensorRow tensor_list;
  rc = di.FetchNextTensorRow(&tensor_list);
  ASSERT_TRUE(rc.IsOk());

  int row_count = 0;
  while (!tensor_list.empty()) {
    // Display the tensor by calling the printer on it
    for (int i = 0; i < tensor_list.size(); i++) {
      std::ostringstream ss;
      ss << "(" << tensor_list[i] << "): " << *tensor_list[i] << std::endl;
      MS_LOG(INFO) << "Tensor print: " << ss.str() << ".";
    }

    rc = di.FetchNextTensorRow(&tensor_list);
    ASSERT_TRUE(rc.IsOk());
    row_count++;
  }

  ASSERT_EQ(row_count, 12);
}

TEST_F(MindDataTestTFReaderOp, TestTFReaderRepeat) {
  // Start with an empty execution tree
  auto my_tree = std::make_shared<ExecutionTree>();
  Status rc;
  std::string dataset_path;
  dataset_path = datasets_root_path_ + "/testTFTestAllTypes/test.data";

  // TFReaderOp
  std::shared_ptr<ConfigManager> config_manager = GlobalContext::config_manager();
  int32_t op_connector_size = config_manager->op_connector_size();
  int32_t num_workers = 1;
  int32_t worker_connector_size = 16;
  std::unique_ptr<DataSchema> schema = std::make_unique<DataSchema>();
  std::vector<std::string> files = {dataset_path};
  std::vector<std::string> columns_to_load = {};

  schema->LoadSchemaFile(datasets_root_path_ + "/testTFTestAllTypes/datasetSchema.json", {});
  std::shared_ptr<TFReaderOp> my_tfreader_op =
    std::make_shared<TFReaderOp>(num_workers, worker_connector_size, 0, files, std::move(schema), op_connector_size,
                                 columns_to_load, false, 1, 0, false);
  rc = my_tfreader_op->Init();
  ASSERT_TRUE(rc.IsOk());
  rc = my_tree->AssociateNode(my_tfreader_op);
  ASSERT_TRUE(rc.IsOk());

  // RepeatOp
  uint32_t num_repeats = 3;
  std::shared_ptr<RepeatOp> my_repeat_op = std::make_shared<RepeatOp>(num_repeats);
  rc = my_tree->AssociateNode(my_repeat_op);
  ASSERT_TRUE(rc.IsOk());

  // Set children/root layout.
  my_tfreader_op->SetTotalRepeats(num_repeats);
  my_tfreader_op->SetNumRepeatsPerEpoch(num_repeats);
  rc = my_repeat_op->AddChild(my_tfreader_op);
  ASSERT_TRUE(rc.IsOk());
  rc = my_tree->AssignRoot(my_repeat_op);
  ASSERT_TRUE(rc.IsOk());

  MS_LOG(INFO) << "Launching tree and begin iteration.";
  rc = my_tree->Prepare();
  ASSERT_TRUE(rc.IsOk());

  rc = my_tree->Launch();
  ASSERT_TRUE(rc.IsOk());

  // Start the loop of reading tensors from our pipeline
  DatasetIterator di(my_tree);
  TensorRow tensor_list;
  rc = di.FetchNextTensorRow(&tensor_list);
  ASSERT_TRUE(rc.IsOk());

  int row_count = 0;
  while (!tensor_list.empty()) {
    MS_LOG(INFO) << "Row display for row #: " << row_count << ".";

    // Display the tensor by calling the printer on it
    for (int i = 0; i < tensor_list.size(); i++) {
      std::ostringstream ss;
      ss << "(" << tensor_list[i] << "): " << *tensor_list[i] << std::endl;
      MS_LOG(INFO) << "Tensor print: " << ss.str() << ".";
    }

    rc = di.FetchNextTensorRow(&tensor_list);
    ASSERT_TRUE(rc.IsOk());
    row_count++;
  }

  ASSERT_EQ(row_count, 12 * 3);
}

TEST_F(MindDataTestTFReaderOp, TestTFReaderSchemaConstructor) {
  // Start with an empty execution tree
  auto my_tree = std::make_shared<ExecutionTree>();
  Status rc;
  std::string dataset_path;
  dataset_path = datasets_root_path_ + "/testTFTestAllTypes";
  std::vector<std::string> files = {dataset_path + "/test.data"};

  std::unique_ptr<DataSchema> schema = std::make_unique<DataSchema>();
  std::vector<std::string> columns_to_load;
  columns_to_load.push_back("col_sint32");
  columns_to_load.push_back("col_binary");
  schema->LoadSchemaFile(dataset_path + "/datasetSchema.json", columns_to_load);

  std::shared_ptr<ConfigManager> config_manager = GlobalContext::config_manager();
  int32_t op_connector_size = config_manager->op_connector_size();
  int32_t worker_connector_size = config_manager->worker_connector_size();
  int32_t num_workers = 1;

  std::shared_ptr<TFReaderOp> my_tfreader_op =
    std::make_shared<TFReaderOp>(num_workers, worker_connector_size, 0, files, std::move(schema), op_connector_size,
                                 columns_to_load, false, 1, 0, false);
  rc = my_tfreader_op->Init();
  ASSERT_TRUE(rc.IsOk());
  rc = my_tree->AssociateNode(my_tfreader_op);
  ASSERT_TRUE(rc.IsOk());

  rc = my_tree->AssignRoot(my_tfreader_op);
  ASSERT_TRUE(rc.IsOk());

  MS_LOG(INFO) << "Launching tree and begin iteration.";
  rc = my_tree->Prepare();
  ASSERT_TRUE(rc.IsOk());

  rc = my_tree->Launch();
  ASSERT_TRUE(rc.IsOk());

  // Start the loop of reading tensors from our pipeline
  DatasetIterator di(my_tree);
  TensorRow tensor_list;
  rc = di.FetchNextTensorRow(&tensor_list);
  ASSERT_TRUE(rc.IsOk());

  int row_count = 0;
  while (!tensor_list.empty()) {
    // Display the tensor by calling the printer on it
    ASSERT_EQ(tensor_list.size(), columns_to_load.size());

    for (int i = 0; i < tensor_list.size(); i++) {
      std::ostringstream ss;
      ss << "(" << tensor_list[i] << "): " << *tensor_list[i] << std::endl;
      MS_LOG(INFO) << "Tensor print: " << ss.str() << ".";
    }

    rc = di.FetchNextTensorRow(&tensor_list);
    ASSERT_TRUE(rc.IsOk());
    row_count++;
  }

  ASSERT_EQ(row_count, 12);
}

TEST_F(MindDataTestTFReaderOp, TestTFReaderTake1Row) {
  // Start with an empty execution tree
  auto my_tree = std::make_shared<ExecutionTree>();
  Status rc;
  std::string dataset_path;
  dataset_path = datasets_root_path_ + "/testTFTestAllTypes";

  std::string data_schema_filepath = dataset_path + "/datasetSchema1Row.json";

  // TFReaderOp
  std::unique_ptr<DataSchema> schema = std::make_unique<DataSchema>();
  schema->LoadSchemaFile(datasets_root_path_ + "/testTFTestAllTypes/datasetSchema1Row.json", {});
  std::shared_ptr<ConfigManager> config_manager = GlobalContext::config_manager();
  int32_t op_connector_size = config_manager->op_connector_size();
  int32_t num_workers = 1;
  int32_t worker_connector_size = config_manager->worker_connector_size();
  std::vector<std::string> files = {dataset_path + "/test.data"};
  std::vector<std::string> columns_to_load = {};

  std::shared_ptr<TFReaderOp> my_tfreader_op =
    std::make_shared<TFReaderOp>(num_workers, worker_connector_size, 0, files, std::move(schema), op_connector_size,
                                 columns_to_load, false, 1, 0, false);
  rc = my_tfreader_op->Init();
  ASSERT_TRUE(rc.IsOk());
  rc = my_tree->AssociateNode(my_tfreader_op);
  ASSERT_TRUE(rc.IsOk());

  rc = my_tree->AssignRoot(my_tfreader_op);
  ASSERT_TRUE(rc.IsOk());

  MS_LOG(INFO) << "Launching tree and begin iteration.";
  rc = my_tree->Prepare();
  ASSERT_TRUE(rc.IsOk());

  rc = my_tree->Launch();
  ASSERT_TRUE(rc.IsOk());

  // Start the loop of reading tensors from our pipeline
  DatasetIterator di(my_tree);
  TensorRow tensor_list;
  rc = di.FetchNextTensorRow(&tensor_list);
  ASSERT_TRUE(rc.IsOk());

  int row_count = 0;
  while (!tensor_list.empty()) {
    MS_LOG(INFO) << "Row display for row #: " << row_count << ".";

    // Display the tensor by calling the printer on it
    for (int i = 0; i < tensor_list.size(); i++) {
      std::ostringstream ss;
      ss << "(" << tensor_list[i] << "): " << *tensor_list[i] << std::endl;
      MS_LOG(INFO) << "Tensor print: " << ss.str() << ".";
    }

    rc = di.FetchNextTensorRow(&tensor_list);
    ASSERT_TRUE(rc.IsOk());
    row_count++;
  }

  ASSERT_EQ(row_count, 1);
}

TEST_F(MindDataTestTFReaderOp, TestTFReaderTake1Buffer) {
  // Start with an empty execution tree
  auto my_tree = std::make_shared<ExecutionTree>();
  Status rc;
  std::string dataset_path;
  dataset_path = datasets_root_path_ + "/testTFTestAllTypes";

  std::string data_schema_filepath = dataset_path + "/datasetSchema5Rows.json";

  // TFReaderOp
  std::unique_ptr<DataSchema> schema = std::make_unique<DataSchema>();
  schema->LoadSchemaFile(datasets_root_path_ + "/testTFTestAllTypes/datasetSchema5Rows.json", {});
  std::shared_ptr<ConfigManager> config_manager = GlobalContext::config_manager();
  int32_t op_connector_size = config_manager->op_connector_size();
  int32_t num_workers = 1;
  int32_t worker_connector_size = config_manager->worker_connector_size();
  std::vector<std::string> files = {dataset_path + "/test.data"};
  std::vector<std::string> columns_to_load = {};

  std::shared_ptr<TFReaderOp> my_tfreader_op =
    std::make_shared<TFReaderOp>(num_workers, worker_connector_size, 0, files, std::move(schema), op_connector_size,
                                 columns_to_load, false, 1, 0, false);
  rc = my_tfreader_op->Init();
  ASSERT_TRUE(rc.IsOk());
  rc = my_tree->AssociateNode(my_tfreader_op);
  ASSERT_TRUE(rc.IsOk());

  rc = my_tree->AssignRoot(my_tfreader_op);
  ASSERT_TRUE(rc.IsOk());

  MS_LOG(INFO) << "Launching tree and begin iteration.";
  rc = my_tree->Prepare();
  ASSERT_TRUE(rc.IsOk());

  rc = my_tree->Launch();
  ASSERT_TRUE(rc.IsOk());

  // Start the loop of reading tensors from our pipeline
  DatasetIterator di(my_tree);
  TensorRow tensor_list;
  rc = di.FetchNextTensorRow(&tensor_list);
  ASSERT_TRUE(rc.IsOk());

  int row_count = 0;
  while (!tensor_list.empty()) {
    MS_LOG(INFO) << "Row display for row #: " << row_count << ".";

    // Display the tensor by calling the printer on it
    for (int i = 0; i < tensor_list.size(); i++) {
      std::ostringstream ss;
      ss << "(" << tensor_list[i] << "): " << *tensor_list[i] << std::endl;
      MS_LOG(INFO) << "Tensor print: " << ss.str() << ".";
    }

    rc = di.FetchNextTensorRow(&tensor_list);
    ASSERT_TRUE(rc.IsOk());
    row_count++;
  }

  ASSERT_EQ(row_count, 5);
}

TEST_F(MindDataTestTFReaderOp, TestTFReaderTake7Rows) {
  // Start with an empty execution tree
  auto my_tree = std::make_shared<ExecutionTree>();
  Status rc;
  std::string dataset_path;
  dataset_path = datasets_root_path_ + "/testTFTestAllTypes";

  std::string data_schema_filepath = dataset_path + "/datasetSchema7Rows.json";

  // TFReaderOp
  std::unique_ptr<DataSchema> schema = std::make_unique<DataSchema>();
  schema->LoadSchemaFile(datasets_root_path_ + "/testTFTestAllTypes/datasetSchema7Rows.json", {});
  std::shared_ptr<ConfigManager> config_manager = GlobalContext::config_manager();
  int32_t op_connector_size = config_manager->op_connector_size();
  int32_t num_workers = 1;
  int32_t worker_connector_size = config_manager->worker_connector_size();
  std::vector<std::string> files = {dataset_path + "/test.data"};
  std::vector<std::string> columns_to_load = {};

  std::shared_ptr<TFReaderOp> my_tfreader_op =
    std::make_shared<TFReaderOp>(num_workers, worker_connector_size, 0, files, std::move(schema), op_connector_size,
                                 columns_to_load, false, 1, 0, false);
  rc = my_tfreader_op->Init();
  ASSERT_TRUE(rc.IsOk());
  rc = my_tree->AssociateNode(my_tfreader_op);
  ASSERT_TRUE(rc.IsOk());

  rc = my_tree->AssignRoot(my_tfreader_op);
  ASSERT_TRUE(rc.IsOk());

  MS_LOG(INFO) << "Launching tree and begin iteration.";
  rc = my_tree->Prepare();
  ASSERT_TRUE(rc.IsOk());

  rc = my_tree->Launch();
  ASSERT_TRUE(rc.IsOk());

  // Start the loop of reading tensors from our pipeline
  DatasetIterator di(my_tree);
  TensorRow tensor_list;
  rc = di.FetchNextTensorRow(&tensor_list);
  ASSERT_TRUE(rc.IsOk());

  int row_count = 0;
  while (!tensor_list.empty()) {
    MS_LOG(INFO) << "Row display for row #: " << row_count << ".";

    // Display the tensor by calling the printer on it
    for (int i = 0; i < tensor_list.size(); i++) {
      std::ostringstream ss;
      ss << "(" << tensor_list[i] << "): " << *tensor_list[i] << std::endl;
      MS_LOG(INFO) << "Tensor print: " << ss.str() << ".";
    }

    rc = di.FetchNextTensorRow(&tensor_list);
    ASSERT_TRUE(rc.IsOk());
    row_count++;
  }

  ASSERT_EQ(row_count, 7);
}

TEST_F(MindDataTestTFReaderOp, TestTFReaderBasicNoSchema) {
  // Start with an empty execution tree
  auto my_tree = std::make_shared<ExecutionTree>();
  Status rc;
  std::string dataset_path;
  dataset_path = datasets_root_path_ + "/testTFTestAllTypes/test.data";
  std::shared_ptr<ConfigManager> config_manager = GlobalContext::config_manager();
  int32_t op_connector_size = config_manager->op_connector_size();
  int32_t num_workers = 1;
  std::vector<std::string> columns_to_load = {};
  std::vector<std::string> files = {dataset_path};
  int32_t worker_connector_size = config_manager->worker_connector_size();
  std::unique_ptr<DataSchema> schema = std::make_unique<DataSchema>();
  std::shared_ptr<TFReaderOp> my_tfreader_op =
    std::make_shared<TFReaderOp>(num_workers, worker_connector_size, 0, files, std::move(schema), op_connector_size,
                                 columns_to_load, false, 1, 0, false);
  rc = my_tfreader_op->Init();
  ASSERT_TRUE(rc.IsOk());
  rc = my_tree->AssociateNode(my_tfreader_op);
  ASSERT_TRUE(rc.IsOk());

  rc = my_tree->AssignRoot(my_tfreader_op);
  ASSERT_TRUE(rc.IsOk());

  MS_LOG(INFO) << "Launching tree and begin iteration.";
  rc = my_tree->Prepare();
  ASSERT_TRUE(rc.IsOk());

  rc = my_tree->Launch();
  ASSERT_TRUE(rc.IsOk());

  // Start the loop of reading tensors from our pipeline
  DatasetIterator di(my_tree);
  TensorRow tensor_list;
  rc = di.FetchNextTensorRow(&tensor_list);
  ASSERT_TRUE(rc.IsOk());

  int row_count = 0;
  while (!tensor_list.empty()) {
    // Display the tensor by calling the printer on it
    ASSERT_EQ(tensor_list.size(), 9);
    for (int i = 0; i < tensor_list.size(); i++) {
      std::ostringstream ss;
      ss << "(" << tensor_list[i] << "): " << *tensor_list[i] << std::endl;
      MS_LOG(INFO) << "Tensor print: " << ss.str() << ".";
    }

    rc = di.FetchNextTensorRow(&tensor_list);
    ASSERT_TRUE(rc.IsOk());
    row_count++;
  }

  ASSERT_EQ(row_count, 12);
}

TEST_F(MindDataTestTFReaderOp, TestTotalRowsBasic) {
  std::string tf_file = datasets_root_path_ + "/testTFTestAllTypes/test.data";

  std::vector<std::string> filenames;

  for (int i = 0; i < 5; i++) {
    filenames.push_back(tf_file);
  }

  int64_t total_rows = 0;
  TFReaderOp::CountTotalRows(&total_rows, filenames, 1);
  ASSERT_EQ(total_rows, 60);
  TFReaderOp::CountTotalRows(&total_rows, filenames, 2);
  ASSERT_EQ(total_rows, 60);
  TFReaderOp::CountTotalRows(&total_rows, filenames, 3);
  ASSERT_EQ(total_rows, 60);
  TFReaderOp::CountTotalRows(&total_rows, filenames, 4);
  ASSERT_EQ(total_rows, 60);
  TFReaderOp::CountTotalRows(&total_rows, filenames, 5);
  ASSERT_EQ(total_rows, 60);
  TFReaderOp::CountTotalRows(&total_rows, filenames, 6);
  ASSERT_EQ(total_rows, 60);
  TFReaderOp::CountTotalRows(&total_rows, filenames, 729);
  ASSERT_EQ(total_rows, 60);
  TFReaderOp::CountTotalRows(&total_rows, filenames, 1, true);
  ASSERT_EQ(total_rows, 60);
  TFReaderOp::CountTotalRows(&total_rows, filenames, 2, true);
  ASSERT_EQ(total_rows, 60);
  TFReaderOp::CountTotalRows(&total_rows, filenames, 3, true);
  ASSERT_EQ(total_rows, 60);
  TFReaderOp::CountTotalRows(&total_rows, filenames, 4, true);
  ASSERT_EQ(total_rows, 60);
  TFReaderOp::CountTotalRows(&total_rows, filenames, 5, true);
  ASSERT_EQ(total_rows, 60);
  TFReaderOp::CountTotalRows(&total_rows, filenames, 6, true);
  ASSERT_EQ(total_rows, 60);
  TFReaderOp::CountTotalRows(&total_rows, filenames, 729, true);
  ASSERT_EQ(total_rows, 60);
}
