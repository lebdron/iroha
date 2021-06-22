/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "consensus/yac/yac.hpp"

#include <utility>

#include <boost/algorithm/string/join.hpp>
#include <boost/range/adaptor/transformed.hpp>
#include "common/bind.hpp"
#include "common/visitor.hpp"
#include "consensus/yac/cluster_order.hpp"
#include "consensus/yac/storage/yac_proposal_storage.hpp"
#include "consensus/yac/timer.hpp"
#include "consensus/yac/yac_crypto_provider.hpp"
#include "interfaces/common_objects/peer.hpp"
#include "logger/logger.hpp"

// TODO: 2019-03-04 @muratovv refactor std::vector<VoteMessage> with a
// separate class IR-374
auto &getRound(const std::vector<iroha::consensus::yac::VoteMessage> &state) {
  return state.at(0).hash.vote_round;
}

namespace iroha {
  namespace consensus {
    namespace yac {

      std::shared_ptr<Yac> Yac::create(
          YacVoteStorage vote_storage,
          std::shared_ptr<YacNetwork> network,
          std::shared_ptr<YacCryptoProvider> crypto,
          std::shared_ptr<Timer> timer,
          std::shared_ptr<LedgerState const> ledger_state,
          logger::LoggerPtr log) {
        return std::make_shared<Yac>(vote_storage,
                                     network,
                                     crypto,
                                     timer,
                                     ledger_state,
                                     std::move(log));
      }

      Yac::Yac(YacVoteStorage vote_storage,
               std::shared_ptr<YacNetwork> network,
               std::shared_ptr<YacCryptoProvider> crypto,
               std::shared_ptr<Timer> timer,
               std::shared_ptr<LedgerState const> ledger_state,
               logger::LoggerPtr log)
          : log_(std::move(log)),
            ledger_state_(ledger_state),
            vote_storage_(std::move(vote_storage)),
            network_(std::move(network)),
            crypto_(std::move(crypto)),
            timer_(std::move(timer)) {}

      void Yac::stop() {
        network_->stop();
      }

      void Yac::processLedgerState(
          std::shared_ptr<LedgerState const> ledger_state) {
        ledger_state_ = ledger_state;
      }

      // ------|Hash gate|------

      void Yac::vote(YacHash hash,
                     ClusterOrdering order) {
        log_->info("Order for voting: [{}]",
                   boost::algorithm::join(
                       order.getPeers()
                           | boost::adaptors::transformed(
                                 [](const auto &p) { return p->address(); }),
                       ", "));

        auto vote = crypto_->getVote(hash);
        // TODO 10.06.2018 andrei: IR-1407 move YAC propagation strategy to a
        // separate entity
        votingStep(vote, std::move(order));
      }

      // ------|Network notifications|------

      std::optional<Answer> Yac::onState(std::vector<VoteMessage> state) {
        if (crypto_->verify(state)) {
          auto &proposal_round = getRound(state);

          if (proposal_round.block_round
              > ledger_state_->top_block_info.height) {
            log_->info("Pass state from future for {} to pipeline",
                       proposal_round);
            return FutureMessage{std::move(state)};
          }

          if (proposal_round.block_round
              < ledger_state_->top_block_info.height) {
            log_->info("Received state from past for {}, try to propagate back",
                       proposal_round);
            tryPropagateBack(state);
            return std::nullopt;
          }

          return applyState(state);
        }

        log_->warn("Crypto verification failed for message. Votes: [{}]",
                   boost::algorithm::join(
                       state | boost::adaptors::transformed([](const auto &v) {
                         return v.signature->toString();
                       }),
                       ", "));
        return std::nullopt;
      }

      // ------|Private interface|------

      void Yac::votingStep(VoteMessage vote, ClusterOrdering order, uint32_t attempt) {
        log_->info("votingStep got vote: {}, attempt {}", vote, attempt);

        auto committed = vote_storage_.isCommitted(vote.hash.vote_round);
        if (committed) {
          return;
        }

        enum { kRotatePeriod = 10 };

        if (0 != attempt && 0 == (attempt % kRotatePeriod)) {
          vote_storage_.remove(vote.hash.vote_round);
        }

        /**
         * 3 attempts to build and commit block before we think that round is
         * freezed
         */
        if (attempt == kRotatePeriod) {
          vote.hash.vote_hashes.proposal_hash.clear();
          vote.hash.vote_hashes.block_hash.clear();
          vote.hash.block_signature.reset();
          vote = crypto_->getVote(vote.hash);
        }

        const auto &current_leader = order.currentLeader();

        log_->info("Vote {} to peer {}", vote, current_leader);

        propagateStateDirectly(current_leader, {vote});
        order.switchToNext();

        timer_->invokeAfterDelay(
            [this, vote, order(std::move(order)), attempt] {
              this->votingStep(vote, std::move(order), attempt + 1);
            });
      }

      boost::optional<std::shared_ptr<shared_model::interface::Peer>>
      Yac::findPeer(const VoteMessage &vote) {
        auto peers = cluster_order_.getPeers();
        auto it =
            std::find_if(peers.begin(), peers.end(), [&](const auto &peer) {
              return peer->pubkey() == vote.signature->publicKey();
            });
        return it != peers.end() ? boost::make_optional(std::move(*it))
                                 : boost::none;
      }

      // ------|Apply data|------

      std::optional<Answer> Yac::applyState(
          const std::vector<VoteMessage> &state) {
        auto answer =
            vote_storage_.store(state, ledger_state_->ledger_peers);

        // TODO 10.06.2018 andrei: IR-1407 move YAC propagation strategy to a
        // separate entity

        if (answer) {
          auto &proposal_round = getRound(state);

          /*
           * It is possible that a new peer with an outdated peers list may
           * collect an outcome from a smaller number of peers which are
           * included in set of `f` peers in the system. The new peer will
           * not accept our message with valid supermajority because he
           * cannot apply votes from unknown peers.
           */
          if (state.size() > 1
              or ledger_state_->ledger_peers.size() == 1) {
            // some peer has already collected commit/reject, so it is sent
            if (vote_storage_.getProcessingState(proposal_round)
                == ProposalState::kNotSentNotProcessed) {
              vote_storage_.nextProcessingState(proposal_round);
              log_->info(
                  "Received supermajority of votes for {}, skip "
                  "propagation",
                  proposal_round);
            }
          }

          auto processing_state =
              vote_storage_.getProcessingState(proposal_round);

          auto votes =
              [](const auto &state) -> const std::vector<VoteMessage> & {
            return state.votes;
          };

          switch (processing_state) {
            case ProposalState::kNotSentNotProcessed:
              vote_storage_.nextProcessingState(proposal_round);
              log_->info("Propagate state {} to whole network", proposal_round);
              propagateState(visit_in_place(*answer, votes));
              break;
            case ProposalState::kSentNotProcessed:
              vote_storage_.nextProcessingState(proposal_round);
              log_->info("Pass outcome for {} to pipeline", proposal_round);
              return *answer;
            case ProposalState::kSentProcessed:
              tryPropagateBack(state);
              break;
          }
        }
        return std::nullopt;
      }

      void Yac::tryPropagateBack(const std::vector<VoteMessage> &state) {
        // yac back propagation will work only if another peer is in
        // propagation stage because if peer sends list of votes this means that
        // state is already committed
        if (state.size() != 1) {
          return;
        }

        vote_storage_.getLastFinalizedRound() | [&](const auto &last_round) {
          if (getRound(state) <= last_round) {
            vote_storage_.getState(last_round) | [&](const auto &last_state) {
              this->findPeer(state.at(0)) | [&](const auto &from) {
                log_->info("Propagate state {} directly to {}",
                           last_round,
                           from->address());
                auto votes = [](const auto &state) { return state.votes; };
                this->propagateStateDirectly(*from,
                                             visit_in_place(last_state, votes));
              };
            };
          }
        };
      }

      // ------|Propagation|------

      void Yac::propagateState(const std::vector<VoteMessage> &msg) {
        for (const auto &peer : ledger_state_->ledger_peers) {
          propagateStateDirectly(*peer, msg);
        }
      }

      void Yac::propagateStateDirectly(const shared_model::interface::Peer &to,
                                       const std::vector<VoteMessage> &msg) {
        network_->sendState(to, msg);
      }

    }  // namespace yac
  }    // namespace consensus
}  // namespace iroha
