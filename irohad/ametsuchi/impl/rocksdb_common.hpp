/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef IROHA_ROCKSDB_COMMON_HPP
#define IROHA_ROCKSDB_COMMON_HPP

#include <charconv>
#include <string>
#include <string_view>

#include <fmt/compile.h>
#include <fmt/format.h>
#include <rocksdb/utilities/transaction.h>
#include "interfaces/common_objects/types.hpp"

namespace {
  auto constexpr kValue{FMT_STRING("{}")};
}

namespace iroha::ametsuchi {

  namespace fmtstrings {
    static auto constexpr kQuorum{
        FMT_STRING("quorum/{}/{}")};  // domain_id/account_name ➡️ quorum
    static auto constexpr kAccountRole{FMT_STRING(
        "account_role/{}/{}/{}")};  // domain_id/account_name/role_name
                                    // ➡️ permissions
    static auto constexpr kRole{
        FMT_STRING("role/{}")};  // role_name ➡️ permissions
    static auto constexpr kDomain{
        FMT_STRING("domain/{}")};  // domain_id ➡️ default role
    static auto constexpr kSignatory{FMT_STRING(
        "signatory/{}/{}/{}")};  // domain_id/account_name/pubkey ➡️ ""
    static auto constexpr kAsset{
        FMT_STRING("asset/{}/{}")};  // domain_id/asset_name ➡️ precision
    static auto constexpr kAccountAsset{FMT_STRING(
        "account_asset/{}/{}/{}")};  // account_domain_id/account_name/asset_id
                                     // ➡️ amount
    static auto constexpr kAccountAssetSize{
        FMT_STRING("account_asset_size/{}/{}")};  // account_domain_id/account_name
                                                  // ➡️ size
    static auto constexpr kAccountDetail{FMT_STRING(
        "account_detail/{}/{}/{}/{}/{}")};  // domain_id/account_name/writer_domain_id/writer_account_name/key
                                            // ➡️ value
    static auto constexpr kPeer{
        FMT_STRING("peer/{}")};  // pubkey ➡️ address
    static auto constexpr kPermissions{FMT_STRING(
        "permissions/{}/{}")};  // domain_id/account_name ➡️ permissions
    static auto constexpr kGranted{FMT_STRING(
        "granted/{}/{}/{}/{}")};  // domain_id/account_name/grantee_domain_id/grantee_account_name
                                  // ➡️ permissions
    static auto constexpr kSetting{
        FMT_STRING("setiing/{}")};  // key ➡️ value
  }  // namespace fmtstrings

  class RocksDbCommon {
   public:
    RocksDbCommon(rocksdb::Transaction &db_transaction,
                  fmt::memory_buffer &key_buffer,
                  std::string &value_buffer)
        : db_transaction_(db_transaction),
          key_buffer_(key_buffer),
          value_buffer_(value_buffer) {
      key_buffer_.clear();
      value_buffer_.clear();
    }

    auto encode(uint64_t number) {
      value_buffer_.clear();
      fmt::format_to(std::back_inserter(value_buffer_), kValue, number);
    }

    auto decode(uint64_t &number) {
      return std::from_chars(value_buffer_.data(),
                             value_buffer_.data() + value_buffer_.size(),
                             number);
    }

    template <typename S, typename... Args>
    auto get(S &fmtstring, Args &&... args) {
      key_buffer_.clear();
      fmt::format_to(key_buffer_, fmtstring, args...);

      value_buffer_.clear();
      return db_transaction_.Get(
          rocksdb::ReadOptions(),
          std::string_view(key_buffer_.data(), key_buffer_.size()),
          &value_buffer_);
    }

    template <typename S, typename... Args>
    auto put(S &fmtstring, Args &&... args) {
      key_buffer_.clear();
      fmt::format_to(key_buffer_, fmtstring, args...);

      return db_transaction_.Put(
          std::string_view(key_buffer_.data(), key_buffer_.size()),
          value_buffer_);
    }

    template <typename S, typename... Args>
    auto del(S &fmtstring, Args &&... args) {
      key_buffer_.clear();
      fmt::format_to(key_buffer_, fmtstring, args...);

      return db_transaction_.Delete(
          std::string_view(key_buffer_.data(), key_buffer_.size()));
    }

    template <typename S, typename... Args>
    auto seek(S &fmtstring, Args &&... args) {
      key_buffer_.clear();
      fmt::format_to(key_buffer_, fmtstring, args...);

      std::unique_ptr<rocksdb::Iterator> it;

      it.reset(db_transaction_.GetIterator(rocksdb::ReadOptions()));

      it->Seek(std::string_view(key_buffer_.data(), key_buffer_.size()));

      return it;
    }

   private:
    rocksdb::Transaction &db_transaction_;
    fmt::memory_buffer &key_buffer_;
    std::string &value_buffer_;
  };

}  // namespace iroha::ametsuchi

#endif
