/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef TEST_INTEGRATION_EXECUTOR_FIXTURE_PARAM_ROCKSDB_HPP
#define TEST_INTEGRATION_EXECUTOR_FIXTURE_PARAM_ROCKSDB_HPP

#include "integration/executor/executor_fixture_param.hpp"

namespace rocksdb {
  class OptimisticTransactionDB;
  class Transaction;
}  // namespace rocksdb

namespace iroha::ametsuchi {
  class MockBlockStorage;
}  // namespace iroha::ametsuchi

namespace executor_testing {

  class RocksDbExecutorTestParam : public ExecutorTestParam {
   public:
    RocksDbExecutorTestParam();

    virtual ~RocksDbExecutorTestParam();

    void clearBackendState() override;

    iroha::integration_framework::ExecutorItfTarget getExecutorItfParam()
        const override;

    std::unique_ptr<iroha::ametsuchi::BurrowStorage> makeBurrowStorage(
        std::string const &tx_hash,
        shared_model::interface::types::CommandIndexType cmd_index)
        const override;

    std::shared_ptr<iroha::ametsuchi::BlockIndex> getBlockIndexer()
        const override;

    std::string toString() const override;

   private:
    std::unique_ptr<iroha::ametsuchi::MockBlockStorage> block_storage_;
    std::string const db_name_;
    std::unique_ptr<rocksdb::OptimisticTransactionDB> transaction_db_;
    std::unique_ptr<rocksdb::Transaction> transaction_;
    iroha::integration_framework::ExecutorItfTarget executor_itf_target_;
    std::shared_ptr<iroha::ametsuchi::BlockIndex> block_indexer_;
  };

  std::reference_wrapper<ExecutorTestParam> getExecutorTestParamRocksDb();
}  // namespace executor_testing

#endif /* TEST_INTEGRATION_EXECUTOR_FIXTURE_PARAM_ROCKSDB_HPP */
