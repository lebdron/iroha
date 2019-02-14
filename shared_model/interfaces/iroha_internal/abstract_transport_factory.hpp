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
  }
}  // namespace iroha

namespace shared_model {
  namespace interface {

    template <typename Interface, typename Transport>
    class AbstractTransportFactory {
     public:
      struct Error {
        types::HashType hash;
        std::string error;
      };

      using BuildResultValue = std::unique_ptr<Interface>;

      virtual iroha::expected::Result<BuildResultValue, Error> build(
          Transport transport) const = 0;

      virtual ~AbstractTransportFactory() = default;
    };

    template <>
    class AbstractTransportFactory<Transaction, iroha::protocol::Transaction> {
     public:
      struct Error {
        types::HashType hash;
        std::string error;
      };

      using BuildResultValue = UniquePtrCounter<Transaction>;

      virtual iroha::expected::Result<BuildResultValue, Error> build(
          iroha::protocol::Transaction transport) const = 0;

      virtual ~AbstractTransportFactory() = default;
    };

  }  // namespace interface
}  // namespace shared_model

#endif  // IROHA_ABSTRACT_TRANSPORT_FACTORY_HPP
