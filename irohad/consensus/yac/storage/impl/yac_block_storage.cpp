/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "consensus/yac/storage/yac_block_storage.hpp"

#include "logger/logger.hpp"

namespace iroha {
  namespace consensus {
    namespace yac {

      // --------| Public API |--------

      YacBlockStorage::YacBlockStorage(
          YacHash hash,
          shared_model::interface::types::PeerList const & peers,
          std::shared_ptr<SupermajorityChecker> supermajority_checker,
          logger::LoggerPtr log)
          : storage_key_(std::move(hash)),
            peers_(peers),
            supermajority_checker_(std::move(supermajority_checker)),
            log_(std::move(log)) {}

      boost::optional<Answer> YacBlockStorage::insert(VoteMessage msg) {
        if (validScheme(msg) and uniqueVote(msg)) {
          votes_.push_back(msg);

          log_->info(
              "Vote with round {} and hashes ({}, {}) inserted, votes in "
              "storage [{}/{}]",
              msg.hash.vote_round,
              msg.hash.vote_hashes.proposal_hash,
              msg.hash.vote_hashes.block_hash,
              votes_.size(),
              peers_.size());
        }
        return getState();
      }

      boost::optional<Answer> YacBlockStorage::insert(
          std::vector<VoteMessage> votes) {
        std::for_each(votes.begin(), votes.end(), [this](auto vote) {
          this->insert(vote);
        });
        return getState();
      }

      std::vector<VoteMessage> YacBlockStorage::getVotes() const {
        return votes_;
      }

      size_t YacBlockStorage::getNumberOfVotes() const {
        return votes_.size();
      }

      boost::optional<Answer> YacBlockStorage::getState() {
        auto supermajority = supermajority_checker_->hasSupermajority(
            votes_.size(), peers_.size());
        if (supermajority) {
          return Answer(CommitMessage(votes_));
        }
        return boost::none;
      }

      bool YacBlockStorage::isContains(const VoteMessage &msg) const {
        return std::count(votes_.begin(), votes_.end(), msg) != 0;
      }

      YacHash YacBlockStorage::getStorageKey() const {
        return storage_key_;
      }

      // --------| private api |--------

      bool YacBlockStorage::uniqueVote(VoteMessage &msg) {
        // lookup take O(n) times
        return std::all_of(votes_.begin(), votes_.end(), [&msg](auto vote) {
          return vote != msg;
        });
      }

      bool YacBlockStorage::validScheme(VoteMessage &vote) {
        auto known_peer = std::any_of(
            peers_.begin(), peers_.end(), [&vote](auto const &peer) {
              return vote.signature->publicKey() == peer->pubkey();
            });
        if (not known_peer) {
          log_->warn("Got a vote from an unknown peer: {}", vote);
        }
        return getStorageKey() == vote.hash and known_peer;
      }

    }  // namespace yac
  }    // namespace consensus
}  // namespace iroha
