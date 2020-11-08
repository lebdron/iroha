/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "integration/executor/executor_fixture.hpp"

#include <gtest/gtest.h>
#include "ametsuchi/setting_query.hpp"
#include "framework/common_constants.hpp"
#include "framework/crypto_literals.hpp"
#include "framework/result_gtest_checkers.hpp"
#include "integration/executor/command_permission_test.hpp"
#include "integration/executor/executor_fixture_param_provider.hpp"
#include "interfaces/common_objects/amount.hpp"
#include "module/shared_model/mock_objects_factories/mock_command_factory.hpp"
#include "module/shared_model/mock_objects_factories/mock_query_factory.hpp"

using namespace common_constants;
using namespace executor_testing;
using namespace framework::expected;
using namespace shared_model::interface::types;

using shared_model::interface::permissions::Grantable;
using shared_model::interface::permissions::Role;

using shared_model::interface::Amount;

static const Amount kAmount{std::string{"12.3"}};
static const Amount kZeroAmount{std::string{"0.0"}};
static const std::string kDestUser{"destuser"};
static const std::string kDestDomain{"destdomain"};
static const auto kUserPubkey{"userpubkey"_hex_pubkey};
static const auto kDestUserPubkey{"destuserpubkey"_hex_pubkey};
static const std::string kDestUserId{kDestUser + "@" + kDestDomain};
static const std::string kDescription{"description"};

class TransferAssetTest : public ExecutorTestBase {
 public:
  iroha::ametsuchi::CommandResult transferAsset(
      AccountIdType const &issuer = kAdminId,
      AccountIdType const &source = kUserId,
      AccountIdType const &destination = kDestUserId,
      AssetIdType const &asset = kAssetId,
      DescriptionType const &description = kDescription,
      Amount const &amount = kAmount,
      bool validation_enabled = true) {
    return getItf().executeCommandAsAccount(
        *getItf().getMockCommandFactory()->constructTransferAsset(
            source, destination, asset, description, amount),
        issuer,
        validation_enabled);
  }
};

using TransferAssetBasicTest = BasicExecutorTest<TransferAssetTest>;

/**
 * @given pair of users
 *        AND the second user without can_receive permission
 * @when execute tx with TransferAsset command
 * @then there is an empty verified proposal
 */
TEST_P(TransferAssetBasicTest, WithoutCanReceive) {
  ASSERT_NO_FATAL_FAILURE(createAsset(kAssetName, kDomain, 1));
  ASSERT_NO_FATAL_FAILURE(getItf().createUserWithPerms(
      kUser, kDomain, kUserPubkey, {Role::kTransfer}));
  ASSERT_NO_FATAL_FAILURE(addAsset(kUserId, kAssetId, kAmount));

  ASSERT_NO_FATAL_FAILURE(getItf().createDomain(kDestDomain));
  ASSERT_NO_FATAL_FAILURE(getItf().createUserWithPerms(
      kDestUser, kDestDomain, kDestUserPubkey, {}));

  checkCommandError(transferAsset(), 2);

  checkAssetQuantities(kUserId, {AssetQuantity{kAssetId, kAmount}});
  checkAssetQuantities(kDestUserId, {});
}

/**
 * @given command
 * @when trying to add transfer asset to account with root permission
 * @then account asset is successfully transferred
 */
TEST_P(TransferAssetBasicTest, DestWithRoot) {
  ASSERT_NO_FATAL_FAILURE(createAsset(kAssetName, kDomain, 1));
  ASSERT_NO_FATAL_FAILURE(getItf().createUserWithPerms(
      kUser, kDomain, kUserPubkey, {Role::kTransfer}));
  ASSERT_NO_FATAL_FAILURE(addAsset(kUserId, kAssetId, kAmount));

  ASSERT_NO_FATAL_FAILURE(getItf().createDomain(kDestDomain));
  ASSERT_NO_FATAL_FAILURE(getItf().createUserWithPerms(
      kDestUser, kDestDomain, kDestUserPubkey, {Role::kRoot}));

  IROHA_ASSERT_RESULT_VALUE(transferAsset());

  checkAssetQuantities(kUserId, {AssetQuantity{kAssetId, kZeroAmount}});
  checkAssetQuantities(kDestUserId, {AssetQuantity{kAssetId, kAmount}});
}

/**
 * @given some user with all required permissions
 * @when execute tx with TransferAsset command to nonexistent source
 * @then there is an empty verified proposal
 */
TEST_P(TransferAssetBasicTest, NonexistentSrc) {
  ASSERT_NO_FATAL_FAILURE(createAsset(kAssetName, kDomain, 1));

  ASSERT_NO_FATAL_FAILURE(getItf().createDomain(kDestDomain));
  ASSERT_NO_FATAL_FAILURE(getItf().createUserWithPerms(
      kDestUser, kDestDomain, kDestUserPubkey, {Role::kReceive}));

  checkCommandError(transferAsset(), 3);

  checkAssetQuantities(kDestUserId, {});
}

/**
 * @given some user with all required permissions
 * @when execute tx with TransferAsset command to nonexistent destination
 * @then there is an empty verified proposal
 */
TEST_P(TransferAssetBasicTest, NonexistentDest) {
  ASSERT_NO_FATAL_FAILURE(createAsset(kAssetName, kDomain, 1));
  ASSERT_NO_FATAL_FAILURE(getItf().createUserWithPerms(
      kUser, kDomain, kUserPubkey, {Role::kTransfer}));
  ASSERT_NO_FATAL_FAILURE(addAsset(kUserId, kAssetId, kAmount));

  checkCommandError(transferAsset(), 4);

  checkAssetQuantities(kUserId, {AssetQuantity{kAssetId, kAmount}});
}

/**
 * @given pair of users with all required permissions
 * @when execute tx with TransferAsset command with nonexistent asset
 * @then there is an empty verified proposal
 */
TEST_P(TransferAssetBasicTest, NonexistentAsset) {
  ASSERT_NO_FATAL_FAILURE(getItf().createUserWithPerms(
      kUser, kDomain, kUserPubkey, {Role::kTransfer}));

  ASSERT_NO_FATAL_FAILURE(getItf().createDomain(kDestDomain));
  ASSERT_NO_FATAL_FAILURE(getItf().createUserWithPerms(
      kDestUser, kDestDomain, kDestUserPubkey, {Role::kReceive}));

  checkCommandError(transferAsset(), 5);

  checkAssetQuantities(kUserId, {});
  checkAssetQuantities(kDestUserId, {});
}

/**
 * @given pair of users with all required permissions
 * @when execute tx with TransferAsset command with amount more, than user has
 * @then there is an empty verified proposal
 */
TEST_P(TransferAssetBasicTest, MoreThanHas) {
  ASSERT_NO_FATAL_FAILURE(createAsset(kAssetName, kDomain, 1));
  ASSERT_NO_FATAL_FAILURE(getItf().createUserWithPerms(
      kUser, kDomain, kUserPubkey, {Role::kTransfer}));
  ASSERT_NO_FATAL_FAILURE(addAsset(kUserId, kAssetId, kAmount));

  ASSERT_NO_FATAL_FAILURE(getItf().createDomain(kDestDomain));
  ASSERT_NO_FATAL_FAILURE(getItf().createUserWithPerms(
      kDestUser, kDestDomain, kDestUserPubkey, {Role::kReceive}));

  auto amount = kAmount;
  amount += Amount{"1.0"};
  checkCommandError(
      transferAsset(
          kAdminId, kUserId, kDestUserId, kAssetId, kDescription, amount),
      6);

  checkAssetQuantities(kUserId, {AssetQuantity{kAssetId, kAmount}});
  checkAssetQuantities(kDestUserId, {});
}

/**
 * @given command
 * @when trying to transfer asset that the transmitter does not posess
 * @then account asset fails to be transferred
 */
TEST_P(TransferAssetBasicTest, NoSrcAsset) {
  ASSERT_NO_FATAL_FAILURE(createAsset(kAssetName, kDomain, 1));
  ASSERT_NO_FATAL_FAILURE(getItf().createUserWithPerms(
      kUser, kDomain, kUserPubkey, {Role::kTransfer}));

  ASSERT_NO_FATAL_FAILURE(getItf().createDomain(kDestDomain));
  ASSERT_NO_FATAL_FAILURE(getItf().createUserWithPerms(
      kDestUser, kDestDomain, kDestUserPubkey, {Role::kReceive}));

  checkCommandError(transferAsset(), 6);

  checkAssetQuantities(kUserId, {});
  checkAssetQuantities(kDestUserId, {});
}

/**
 * @given command
 * @when transfer an asset which the receiver already has
 * @then account asset is successfully transferred
 */
TEST_P(TransferAssetBasicTest, DestHasAsset) {
  ASSERT_NO_FATAL_FAILURE(createAsset(kAssetName, kDomain, 1));
  ASSERT_NO_FATAL_FAILURE(getItf().createUserWithPerms(
      kUser, kDomain, kUserPubkey, {Role::kTransfer}));
  ASSERT_NO_FATAL_FAILURE(addAsset(kUserId, kAssetId, kAmount));

  ASSERT_NO_FATAL_FAILURE(getItf().createDomain(kDestDomain));
  ASSERT_NO_FATAL_FAILURE(getItf().createUserWithPerms(
      kDestUser, kDestDomain, kDestUserPubkey, {Role::kReceive}));
  ASSERT_NO_FATAL_FAILURE(addAsset(kDestUserId, kAssetId, kAmount));

  IROHA_ASSERT_RESULT_VALUE(transferAsset());

  auto amount = kAmount;
  amount += kAmount;
  checkAssetQuantities(kUserId, {AssetQuantity{kAssetId, kZeroAmount}});
  checkAssetQuantities(kDestUserId, {AssetQuantity{kAssetId, amount}});
}

/**
 * @given two users with all required permissions, one having the maximum
 * allowed quantity of an asset with precision 1
 * @when execute a tx from another user with TransferAsset command for that
 * asset with the smallest possible quantity and then with a lower one
 * @then the last 2 transactions are not committed
 */
TEST_P(TransferAssetBasicTest, DestOverflowPrecision1) {
  ASSERT_NO_FATAL_FAILURE(createAsset(kAssetName, kDomain, 1));
  ASSERT_NO_FATAL_FAILURE(getItf().createUserWithPerms(
      kUser, kDomain, kUserPubkey, {Role::kTransfer}));
  ASSERT_NO_FATAL_FAILURE(addAsset(kUserId, kAssetId, kAmount));

  ASSERT_NO_FATAL_FAILURE(getItf().createDomain(kDestDomain));
  ASSERT_NO_FATAL_FAILURE(getItf().createUserWithPerms(
      kDestUser, kDestDomain, kDestUserPubkey, {Role::kReceive}));
  ASSERT_NO_FATAL_FAILURE(addAsset(kDestUserId, kAssetId, kAmountPrec1Max));

  checkCommandError(transferAsset(kAdminId,
                                  kUserId,
                                  kDestUserId,
                                  kAssetId,
                                  kDescription,
                                  Amount{"0.1"}),
                    7);
  checkCommandError(
      transferAsset(
          kAdminId, kUserId, kDestUserId, kAssetId, kDescription, Amount{"1"}),
      7);

  checkAssetQuantities(kUserId, {AssetQuantity{kAssetId, kAmount}});
  checkAssetQuantities(kDestUserId, {AssetQuantity{kAssetId, kAmountPrec1Max}});
}

/**
 * @given two users with all required permissions, one having the maximum
 * allowed quantity of an asset with precision 2
 * @when execute a tx from another user with TransferAsset command for that
 * asset with the smallest possible quantity and then with a lower one
 * @then last 2 transactions are not committed
 */
TEST_P(TransferAssetBasicTest, DestOverflowPrecision2) {
  ASSERT_NO_FATAL_FAILURE(createAsset(kAssetName, kDomain, 2));
  ASSERT_NO_FATAL_FAILURE(getItf().createUserWithPerms(
      kUser, kDomain, kUserPubkey, {Role::kTransfer}));
  ASSERT_NO_FATAL_FAILURE(addAsset(kUserId, kAssetId, kAmount));

  ASSERT_NO_FATAL_FAILURE(getItf().createDomain(kDestDomain));
  ASSERT_NO_FATAL_FAILURE(getItf().createUserWithPerms(
      kDestUser, kDestDomain, kDestUserPubkey, {Role::kReceive}));
  ASSERT_NO_FATAL_FAILURE(addAsset(kDestUserId, kAssetId, kAmountPrec2Max));

  checkCommandError(transferAsset(kAdminId,
                                  kUserId,
                                  kDestUserId,
                                  kAssetId,
                                  kDescription,
                                  Amount{"0.01"}),
                    7);
  checkCommandError(transferAsset(kAdminId,
                                  kUserId,
                                  kDestUserId,
                                  kAssetId,
                                  kDescription,
                                  Amount{"0.1"}),
                    7);

  checkAssetQuantities(kUserId, {AssetQuantity{kAssetId, kAmount}});
  checkAssetQuantities(kDestUserId, {AssetQuantity{kAssetId, kAmountPrec2Max}});
}

/**
 * @given pair of users with all required permissions
 * @when execute tx with TransferAsset command with a description longer than
 * iroha::ametsuchi::kMaxDescriptionSizeKey settings value
 * @then the tx hasn't passed stateful validation
 */
TEST_P(TransferAssetBasicTest, LongDesc) {
  ASSERT_NO_FATAL_FAILURE(createAsset(kAssetName, kDomain, 1));
  ASSERT_NO_FATAL_FAILURE(getItf().createUserWithPerms(
      kUser, kDomain, kUserPubkey, {Role::kTransfer}));
  ASSERT_NO_FATAL_FAILURE(addAsset(kUserId, kAssetId, kAmount));

  ASSERT_NO_FATAL_FAILURE(getItf().createDomain(kDestDomain));
  ASSERT_NO_FATAL_FAILURE(getItf().createUserWithPerms(
      kDestUser, kDestDomain, kDestUserPubkey, {Role::kReceive}));

  const size_t max_descr_size_setting{10};
  IROHA_ASSERT_RESULT_VALUE(getItf().executeCommandAsAccount(
      *getItf().getMockCommandFactory()->constructSetSettingValue(
          iroha::ametsuchi::kMaxDescriptionSizeKey,
          std::to_string(max_descr_size_setting)),
      kAdminId,
      false));

  checkCommandError(transferAsset(kAdminId,
                                  kUserId,
                                  kDestUserId,
                                  kAssetId,
                                  std::string(max_descr_size_setting + 1, 'a')),
                    8);

  checkAssetQuantities(kUserId, {AssetQuantity{kAssetId, kAmount}});
  checkAssetQuantities(kDestUserId, {});
}

INSTANTIATE_TEST_SUITE_P(Base,
                         TransferAssetBasicTest,
                         executor_testing::getExecutorTestParams(),
                         executor_testing::paramToString);

using TransferAssetPermissionTest =
    command_permission_test::CommandPermissionTest<TransferAssetTest>;

TEST_P(TransferAssetPermissionTest, CommandPermissionTest) {
  ASSERT_NO_FATAL_FAILURE(getItf().createDomain(kSecondDomain));
  ASSERT_NO_FATAL_FAILURE(createAsset(kAssetName, kDomain, 1));
  ASSERT_NO_FATAL_FAILURE(prepareState({}));
  ASSERT_NO_FATAL_FAILURE(addAsset(kUserId, kAssetId, kAmount));

  ASSERT_NO_FATAL_FAILURE(getItf().createDomain(kDestDomain));
  ASSERT_NO_FATAL_FAILURE(getItf().createUserWithPerms(
      kDestUser, kDestDomain, kDestUserPubkey, {Role::kReceive}));

  if (checkResponse(transferAsset(getActor(),
                                  kUserId,
                                  kDestUserId,
                                  kAssetId,
                                  kDescription,
                                  kAmount,
                                  getValidationEnabled()))) {
    checkAssetQuantities(kUserId, {AssetQuantity{kAssetId, kZeroAmount}});
    checkAssetQuantities(kDestUserId, {AssetQuantity{kAssetId, kAmount}});
  } else {
    checkAssetQuantities(kUserId, {AssetQuantity{kAssetId, kAmount}});
    checkAssetQuantities(kDestUserId, {});
  }
}

INSTANTIATE_TEST_SUITE_P(
    Common,
    TransferAssetPermissionTest,
    command_permission_test::getParams(
        Role::kTransfer, boost::none, boost::none, Grantable::kTransferMyAssets),
    command_permission_test::paramToString);
