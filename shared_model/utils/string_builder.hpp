/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef IROHA_SHARED_MODEL_STRING_BUILDER_HPP
#define IROHA_SHARED_MODEL_STRING_BUILDER_HPP

#include <string>
#include <string_view>

#include "common/to_string.hpp"

namespace shared_model {
  namespace detail {
    /**
     * A simple string builder class for building pretty looking strings
     */
    class PrettyStringBuilder {
     public:
      /**
       * Initializes new string with a provided name
       * @param name - name to initialize
       */
      PrettyStringBuilder &init(std::string_view name);

      /**
       * Inserts new level marker
       */
      PrettyStringBuilder &insertLevel();

      /**
       * Closes new level marker
       */
      PrettyStringBuilder &removeLevel();

      ///  ----------  Single element undecorated append.  ----------  ///

      PrettyStringBuilder &append(std::string_view o);

      template <typename T>
      PrettyStringBuilder &append(const T &o) {
        auto const &str = iroha::to_string::toString(o);
        return append(std::string_view{str});
      }

      ///  ----------     Augmented appending functions.   ----------  ///

      /**
       * Appends new field to string as a "name=value" pair
       * @param name - field name to append
       * @param value - field value
       */
      template <typename Name, typename Value>
      PrettyStringBuilder &appendNamed(const Name &name, const Value &value) {
        appendPartial(name);
        appendPartial(keyValueSeparator);
        return append(iroha::to_string::toString(value));
      }

      /**
       * Finalizes appending and returns constructed string.
       * @return resulted string
       */
      std::string finalize();

     private:
      std::string result_;
      bool need_field_separator_;
      static std::string_view const beginBlockMarker;
      static std::string_view const endBlockMarker;
      static std::string_view const keyValueSeparator;
      static std::string_view const singleFieldsSeparator;
      static std::string_view const initSeparator;
      static std::string_view const spaceSeparator;

      template <typename T>
      inline void appendPartial(T const &value) {
        if (need_field_separator_) {
          result_.append(singleFieldsSeparator);
          need_field_separator_ = false;
        }
        result_.append(value);
      }
    };
  }  // namespace detail
}  // namespace shared_model

#endif  // IROHA_SHARED_MODEL_STRING_BUILDER_HPP
