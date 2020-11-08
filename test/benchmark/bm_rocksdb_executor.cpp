/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <benchmark/benchmark.h>

#include "framework/common_constants.hpp"
#include "framework/executor_itf/executor_itf.hpp"
#include "integration/executor/executor_fixture_param_rocksdb.hpp"
#include "module/shared_model/mock_objects_factories/mock_command_factory.hpp"

using benchmark::State;
using shared_model::interface::permissions::Role;
using namespace common_constants;
using namespace shared_model::interface;
using namespace shared_model::interface::types;

static const RoleIdType kAnotherRole("another_role");

static const Amount kInitialAmount{std::string{"1000000000000.0"}};
static const Amount kTransferAmount{std::string{"1.0"}};
static const std::string kDescription{"description"};

class RocksDbFixture : public benchmark::Fixture {
 public:
  void SetUp(State &) override {
    auto &param = executor_testing::getExecutorTestParamRocksDb().get();
    param.clearBackendState();
    auto executor_itf_result =
        iroha::integration_framework::ExecutorItf::create(
            param.getExecutorItfParam());
    if (not iroha::expected::hasValue(executor_itf_result)) {
      printf("error happened: %s\n", executor_itf_result.assumeError().c_str());
      return;
    }
    executor_itf_ = std::move(executor_itf_result).assumeValue();
  }

  iroha::integration_framework::ExecutorItf &getItf() const {
    return *executor_itf_;
  }

  iroha::ametsuchi::CommandResult createRole(
      const AccountIdType &issuer,
      const shared_model::interface::RolePermissionSet &permissions) {
    return getItf().executeCommandAsAccount(
        *getItf().getMockCommandFactory()->constructCreateRole(
            kAnotherRole + std::to_string(counter++), permissions),
        issuer,
        true);
  }

  iroha::ametsuchi::CommandResult createAsset(const AccountIdType &issuer,
                                              const std::string &name,
                                              const std::string &domain,
                                              PrecisionType precision) const {
    return getItf().executeCommandAsAccount(
        *getItf().getMockCommandFactory()->constructCreateAsset(
            name, domain, precision),
        issuer,
        true);
  }

  iroha::ametsuchi::CommandResult addAsset(const AccountIdType &issuer,
                                           const AssetIdType &asset,
                                           const Amount &amount) {
    return getItf().executeCommandAsAccount(
        *getItf().getMockCommandFactory()->constructAddAssetQuantity(asset,
                                                                     amount),
        issuer,
        true);
  }

  iroha::ametsuchi::CommandResult transferAsset(
      const AccountIdType &issuer,
      AccountIdType const &source,
      AccountIdType const &destination,
      const AssetIdType &asset,
      const DescriptionType &description,
      const Amount &amount) {
    return getItf().executeCommandAsAccount(
        *getItf().getMockCommandFactory()->constructTransferAsset(
            source, destination, asset, description, amount),
        issuer,
        true);
  }

 private:
  std::unique_ptr<iroha::integration_framework::ExecutorItf> executor_itf_;
  int counter;
};

BENCHMARK_F(RocksDbFixture, CreateRole)(State &state) {
  auto result = getItf().createUserWithPerms(
      kUser,
      kDomain,
      PublicKeyHexStringView{kUserKeypair.publicKey()},
      {Role::kCreateRole});
  if (not iroha::expected::hasValue(result)) {
    printf("error happened: %s\n", result.assumeError().toString().c_str());
    return;
  }
  for (auto _ : state) {
    result = createRole(kUserId, {});
    if (not iroha::expected::hasValue(result)) {
      printf("error happened: %s\n", result.assumeError().toString().c_str());
      return;
    }
  }
}

BENCHMARK_F(RocksDbFixture, TransferAsset)(State &state) {
  auto result = getItf().createUserWithPerms(
      kUser,
      kDomain,
      PublicKeyHexStringView{kUserKeypair.publicKey()},
      {Role::kReceive});
  if (not iroha::expected::hasValue(result)) {
    printf("error happened: %s\n", result.assumeError().toString().c_str());
    return;
  }
  result = createAsset(kAdminId, kAssetName, kDomain, 1);
  if (not iroha::expected::hasValue(result)) {
    printf("error happened: %s\n", result.assumeError().toString().c_str());
    return;
  }
  result = addAsset(kAdminId, kAssetId, kInitialAmount);
  if (not iroha::expected::hasValue(result)) {
    printf("error happened: %s\n", result.assumeError().toString().c_str());
    return;
  }
  for (auto _ : state) {
    result = transferAsset(
        kAdminId, kAdminId, kUserId, kAssetId, kDescription, kTransferAmount);
    if (not iroha::expected::hasValue(result)) {
      printf("error happened: %s\n", result.assumeError().toString().c_str());
      return;
    }
  }
}
