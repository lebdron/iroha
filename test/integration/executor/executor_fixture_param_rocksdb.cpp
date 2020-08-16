/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "integration/executor/executor_fixture_param_rocksdb.hpp"

#include <rocksdb/utilities/optimistic_transaction_db.h>
#include <rocksdb/utilities/transaction.h>

#include <boost/filesystem.hpp>

#include "ametsuchi/burrow_storage.hpp"
#include "ametsuchi/impl/rocksdb_command_executor.hpp"
#include "ametsuchi/impl/rocksdb_specific_query_executor.hpp"
#include "backend/protobuf/proto_permission_to_string.hpp"
#include "backend/protobuf/proto_query_response_factory.hpp"
#include "module/irohad/ametsuchi/mock_block_storage.hpp"
#include "module/irohad/ametsuchi/mock_vm_caller.hpp"
#include "module/irohad/pending_txs_storage/pending_txs_storage_mock.hpp"

using namespace executor_testing;
using namespace iroha;
using namespace iroha::ametsuchi;
using namespace iroha::integration_framework;

RocksDbExecutorTestParam::RocksDbExecutorTestParam()
    : block_storage_(std::make_unique<MockBlockStorage>()),
      db_name_((boost::filesystem::temp_directory_path()
                / boost::filesystem::unique_path())
                   .string()) {
  rocksdb::OptimisticTransactionDB *transaction_db;
  rocksdb::Options options;
  options.create_if_missing = true;
  options.error_if_exists = true;
  auto status = rocksdb::OptimisticTransactionDB::Open(
      options, db_name_, &transaction_db);
  if (not status.ok()) {
    throw std::runtime_error(status.ToString());
  }

  transaction_db_.reset(transaction_db);

  transaction_.reset(
      transaction_db_->BeginTransaction(rocksdb::WriteOptions()));

  executor_itf_target_.command_executor =
      std::make_shared<RocksDbCommandExecutor>(
          *transaction_,
          std::make_shared<shared_model::proto::ProtoPermissionToString>(),
          *vm_caller_);
  executor_itf_target_.query_executor =
      std::make_shared<RocksDbSpecificQueryExecutor>(
          *transaction_,
          *block_storage_,
          std::make_shared<MockPendingTransactionStorage>(),
          std::make_shared<shared_model::proto::ProtoQueryResponseFactory>(),
          std::make_shared<shared_model::proto::ProtoPermissionToString>());
}

RocksDbExecutorTestParam::~RocksDbExecutorTestParam() {
  rocksdb::Options options;
  options.create_if_missing = true;
  options.error_if_exists = true;
  transaction_.reset();
  transaction_db_.reset();
  rocksdb::DestroyDB(db_name_, options);
}

void RocksDbExecutorTestParam::clearBackendState() {
  rocksdb::Options options;
  options.create_if_missing = true;
  options.error_if_exists = true;
  transaction_.reset();
  transaction_db_.reset();
  rocksdb::DestroyDB(db_name_, options);

  rocksdb::OptimisticTransactionDB *transaction_db;

  auto status = rocksdb::OptimisticTransactionDB::Open(
      options, db_name_, &transaction_db);
  if (not status.ok()) {
    throw std::runtime_error(status.ToString());
  }

  transaction_db_.reset(transaction_db);

  transaction_.reset(
      transaction_db_->BeginTransaction(rocksdb::WriteOptions()));

  executor_itf_target_.command_executor =
      std::make_shared<RocksDbCommandExecutor>(
          *transaction_,
          std::make_shared<shared_model::proto::ProtoPermissionToString>(),
          *vm_caller_);
  executor_itf_target_.query_executor =
      std::make_shared<RocksDbSpecificQueryExecutor>(
          *transaction_,
          *block_storage_,
          std::make_shared<MockPendingTransactionStorage>(),
          std::make_shared<shared_model::proto::ProtoQueryResponseFactory>(),
          std::make_shared<shared_model::proto::ProtoPermissionToString>());
}

ExecutorItfTarget RocksDbExecutorTestParam::getExecutorItfParam() const {
  return executor_itf_target_;
}

std::unique_ptr<iroha::ametsuchi::BurrowStorage>
RocksDbExecutorTestParam::makeBurrowStorage(
    std::string const &tx_hash,
    shared_model::interface::types::CommandIndexType cmd_index) const {
  return {};
}

std::shared_ptr<iroha::ametsuchi::BlockIndex>
RocksDbExecutorTestParam::getBlockIndexer() const {
  return block_indexer_;
}

std::string RocksDbExecutorTestParam::toString() const {
  return "RocksDB";
}

std::reference_wrapper<ExecutorTestParam>
executor_testing::getExecutorTestParamRocksDb() {
  static RocksDbExecutorTestParam param;
  return param;
}
