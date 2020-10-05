/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <rxcpp/operators/rx-filter.hpp>
#include <rxcpp/operators/rx-observe_on.hpp>
#include <rxcpp/operators/rx-replay.hpp>
#include <rxcpp/operators/rx-take.hpp>
#include <rxcpp/operators/rx-timeout.hpp>

#include "ametsuchi/block_query.hpp"
#include "builders/protobuf/transaction.hpp"
#include "common/bind.hpp"
#include "consensus/yac/vote_message.hpp"
#include "consensus/yac/yac_hash_provider.hpp"
#include "framework/crypto_literals.hpp"
#include "framework/integration_framework/fake_peer/behaviour/honest.hpp"
#include "framework/integration_framework/fake_peer/block_storage.hpp"
#include "framework/integration_framework/iroha_instance.hpp"
#include "framework/integration_framework/test_irohad.hpp"
#include "framework/test_logger.hpp"
#include "integration/acceptance/fake_peer_fixture.hpp"
#include "interfaces/common_objects/peer.hpp"
#include "interfaces/common_objects/string_view_types.hpp"
#include "module/shared_model/builders/protobuf/block.hpp"
#include "module/shared_model/cryptography/crypto_defaults.hpp"
#include "ordering/impl/on_demand_common.cpp"

using namespace common_constants;
using namespace shared_model;
using namespace integration_framework;
using namespace shared_model::interface::permissions;

using interface::types::PublicKeyHexStringView;

class PeerSynchronizationFixture : public FakePeerFixture {
 public:
  auto buildBlock(fake_peer::BlockStorage::HeightType height,
                  crypto::Hash prev_hash) {
    std::vector<shared_model::proto::Transaction> transactions;
    for (int i = 0; i < 10; ++i) {
      transactions.push_back(
          complete(baseTx(kAdminId)
                       .addAssetQuantity(kAssetId, "1.0")
                       .transferAsset(kAdminId, kUserId, kAssetId, "", "1.0"),
                   kAdminKeypair));
    }

    auto block = proto::BlockBuilder()
                     .height(height)
                     .prevHash(prev_hash)
                     .createdTime(getUniqueTime())
                     .transactions(transactions)
                     .build();

    for (auto &key : keys) {
      block.signAndAddSignature(key);
    }

    return std::make_shared<shared_model::proto::Block>(block.finish());
  }

  std::vector<shared_model::crypto::Keypair> keys;
  std::unordered_map<fake_peer::BlockStorage::HeightType, crypto::Hash> hashes;
};

/**
 * @given a network of a single fake peer with a block store containing addPeer
 * command that adds itf peer
 * @when itf peer is brought up
 * @then itf peer gets synchronized, sees itself in the WSV and can commit txs
 */
TEST_F(PeerSynchronizationFixture, RealPeerIsAdded) {
  // create the initial fake peer
  auto initial_peer = itf_->addFakePeer(boost::none);

  keys.push_back(common_constants::kAdminKeypair);
  keys.push_back(initial_peer->getKeypair());

  // instruct the initial fake peer to send a commit when synchronization needed
  using iroha::consensus::yac::YacHash;
  struct SynchronizerBehaviour : public fake_peer::HonestBehaviour {
    SynchronizerBehaviour(YacHash sync_hash,
                          PeerSynchronizationFixture *fixture)
        : sync_hash_(std::move(sync_hash)), fixture(fixture) {}
    void processYacMessage(
        std::shared_ptr<const fake_peer::YacMessage> message) override {
      fake_peer::HonestBehaviour::processYacMessage(message);
      if (not message->empty()) {
        fixture->hashes[message->front().hash.vote_round.block_round] =
            crypto::Hash::fromHexString(
                message->front().hash.vote_hashes.block_hash);
        if (message->front().hash.vote_round.block_round > 2
            and message->front().hash.vote_round.block_round
                <= sync_hash_.vote_round.block_round) {
          using iroha::operator|;
          getFakePeer() | [&](auto fake_peer) {
            fake_peer->sendYacState({fake_peer->makeVote(sync_hash_)});
          };
        }
      }
    }

    fake_peer::LoaderBlocksRequestResult processLoaderBlocksRequest(
        fake_peer::LoaderBlocksRequest request) {
      struct iterator {
        using iterator_category = std::input_iterator_tag;
        using value_type =
            fake_peer::LoaderBlocksRequestResult::iterator::value_type;
        using difference_type = std::ptrdiff_t;
        using pointer = std::add_pointer_t<value_type>;
        using reference =
            std::add_lvalue_reference_t<std::add_const_t<value_type>>;

        iterator() {}

        explicit iterator(fake_peer::BlockStorage::HeightType current_height,
                          fake_peer::BlockStorage::HeightType top_height,
                          PeerSynchronizationFixture *fixture)
            : current_height(current_height),
              top_height(top_height),
              fixture(fixture) {
          block = fixture->buildBlock(current_height,
                                      fixture->hashes[current_height - 1]);
          fixture->hashes[current_height] = block->hash();
        }

        iterator &operator++() {
          assert(block->height() == current_height);
          if (++current_height <= top_height) {
            block = fixture->buildBlock(block->height() + 1, block->hash());
            fixture->hashes[current_height] = block->hash();
            assert(block->height() == current_height);
          } else {
            block.reset();
          }
          return *this;
        }

        iterator operator++(int) {
          iterator ret = *this;
          ++(*this);
          return ret;
        }

        reference operator*() const {
          return block;
        }

        bool operator==(iterator const &other) const {
          return block == other.block;
        }

        bool operator!=(iterator const &other) const {
          return !(*this == other);
        }

        fake_peer::BlockStorage::HeightType current_height;
        fake_peer::BlockStorage::HeightType top_height;
        std::shared_ptr<const shared_model::proto::Block> block;
        PeerSynchronizationFixture *fixture;
      };

      return boost::make_iterator_range(
          iterator{request, sync_hash_.vote_round.block_round, fixture},
          iterator{});
    }

    YacHash sync_hash_;
    PeerSynchronizationFixture *fixture;
  };

  interface::types::HeightType height{100};

  initial_peer->setBehaviour(std::make_shared<SynchronizerBehaviour>(
      YacHash{
          iroha::consensus::Round{height, iroha::ordering::kFirstRejectRound},
          "proposal_hash",
          "block_hash"},
      this));

  // init the itf peer with our genesis block
  auto genesis_block = itf_->defaultBlock();
  itf_->setGenesisBlock(genesis_block);
  hashes[genesis_block.height()] = genesis_block.hash();

  // launch the itf peer
  itf_->run();

  auto permissions = shared_model::interface::RolePermissionSet(
      {shared_model::interface::permissions::Role::kReceive,
       shared_model::interface::permissions::Role::kTransfer});
  itf_->sendTx(makeUserWithPerms(permissions));

  // check that itf peer is synchronized
  itf_->getPcsOnCommitObservable()
      .filter([height](auto const &sync_event) {
        return sync_event.ledger_state->top_block_info.height == height;
      })
      .take(1)
      .as_blocking()
      .subscribe();
}
