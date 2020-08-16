/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ametsuchi/impl/executor_common.hpp"

#include "interfaces/permissions.hpp"

namespace iroha {
  namespace ametsuchi {

    const std::string kRootRolePermStr{
        shared_model::interface::RolePermissionSet(
            {shared_model::interface::permissions::Role::kRoot})
            .toBitstring()};

    std::string_view getDomainFromName(std::string_view account_id) {
      // TODO 03.10.18 andrei: IR-1728 Move getDomainFromName to shared_model
      return splitId(account_id).at(1);
    }

    std::vector<std::string_view> splitId(std::string_view id) {
      std::string_view delims{"@#"};
      std::vector<std::string_view> output;
      output.reserve(2);

      for (auto first = id.data(), second = id.data(), last = first + id.size();
           second != last && first != last;
           first = second + 1) {
        second = std::find_first_of(
            first, last, std::cbegin(delims), std::cend(delims));

        if (first != second)
          output.emplace_back(first, second - first);
      }

      return output;
    }

  }  // namespace ametsuchi
}  // namespace iroha
