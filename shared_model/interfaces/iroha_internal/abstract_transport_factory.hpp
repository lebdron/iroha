/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef IROHA_ABSTRACT_TRANSPORT_FACTORY_HPP
#define IROHA_ABSTRACT_TRANSPORT_FACTORY_HPP

#include <memory>

#include "common/result.hpp"
#include "cryptography/hash.hpp"
#include "interfaces/common_objects/types.hpp"

#include "obj_counter.hpp"

namespace iroha {
  namespace protocol {
    class Transaction;
    class Proposal;
    class Block;
  }
}  // namespace iroha

namespace shared_model {
  namespace interface {

    class Proposal;

    namespace detail {
      template <typename Interface>
      struct BuildResultValueChooser {
        using Type = typename std::unique_ptr<Interface>;
      };

      template <>
      struct BuildResultValueChooser<Proposal> {
        using Type = typename ::UniquePtrCounter<Proposal>;
      };

      template <>
      struct BuildResultValueChooser<Transaction> {
        using Type = typename ::UniquePtrCounter<Transaction>;
      };

      template <>
      struct BuildResultValueChooser<Block> {
        using Type = typename ::UniquePtrCounter<Block>;
      };
    }  // namespace detail

    template <typename Interface, typename Transport>
    class AbstractTransportFactory {
     public:
      struct Error {
        types::HashType hash;
        std::string error;
      };

      using BuildResultValue =
          typename detail::BuildResultValueChooser<Interface>::Type;

      virtual iroha::expected::Result<BuildResultValue, Error> build(
          Transport transport) const = 0;

      virtual ~AbstractTransportFactory() = default;
    };

  }  // namespace interface
}  // namespace shared_model

#endif  // IROHA_ABSTRACT_TRANSPORT_FACTORY_HPP
