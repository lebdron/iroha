/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "simulator/block_creator_common.hpp"

namespace iroha {
  namespace simulator {

    SharedPtrCounter<shared_model::interface::Block> getBlockUnsafe(
        const BlockCreatorEvent &event) {
      return event.round_data->block;
    }

  }  // namespace simulator
}  // namespace iroha
