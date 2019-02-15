/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "network/ordering_gate_common.hpp"

#include "obj_counter.hpp"

namespace iroha {
  namespace network {

    SharedPtrCounter<const shared_model::interface::Proposal> getProposalUnsafe(
        const OrderingEvent &event) {
      return *event.proposal;
    }

  }  // namespace network
}  // namespace iroha
