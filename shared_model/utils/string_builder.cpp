/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "utils/string_builder.hpp"

namespace shared_model {
  namespace detail {

    std::string_view const PrettyStringBuilder::beginBlockMarker = "[";
    std::string_view const PrettyStringBuilder::endBlockMarker = "]";
    std::string_view const PrettyStringBuilder::keyValueSeparator = "=";
    std::string_view const PrettyStringBuilder::singleFieldsSeparator = ", ";
    std::string_view const PrettyStringBuilder::initSeparator = ":";
    std::string_view const PrettyStringBuilder::spaceSeparator = " ";

    PrettyStringBuilder &PrettyStringBuilder::init(std::string_view name) {
      result_.append(name);
      result_.append(initSeparator);
      result_.append(spaceSeparator);
      insertLevel();
      return *this;
    }

    PrettyStringBuilder &PrettyStringBuilder::insertLevel() {
      need_field_separator_ = false;
      result_.append(beginBlockMarker);
      return *this;
    }

    PrettyStringBuilder &PrettyStringBuilder::removeLevel() {
      result_.append(endBlockMarker);
      need_field_separator_ = true;
      return *this;
    }

    PrettyStringBuilder &PrettyStringBuilder::append(std::string_view value) {
      appendPartial(value);
      need_field_separator_ = true;
      return *this;
    }

    std::string PrettyStringBuilder::finalize() {
      removeLevel();
      return result_;
    }

  }  // namespace detail
}  // namespace shared_model
