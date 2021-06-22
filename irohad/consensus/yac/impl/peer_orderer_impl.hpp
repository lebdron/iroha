/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef IROHA_PEER_ORDERER_IMPL_HPP
#define IROHA_PEER_ORDERER_IMPL_HPP

#include <memory>

#include "consensus/yac/yac_peer_orderer.hpp"

namespace iroha {

  namespace consensus {
    namespace yac {

      class ClusterOrdering;
      class YacHash;

      class PeerOrdererImpl : public YacPeerOrderer {
       public:
        boost::optional<ClusterOrdering> getOrdering(
            const YacHash &hash,
            std::vector<std::shared_ptr<shared_model::interface::Peer>> const
                &peers) override;

       private:
        std::vector<size_t> peer_positions_;
      };

    }  // namespace yac
  }    // namespace consensus
}  // namespace iroha

#endif  // IROHA_PEER_ORDERER_IMPL_HPP
