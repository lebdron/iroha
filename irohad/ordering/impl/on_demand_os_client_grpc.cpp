/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ordering/impl/on_demand_os_client_grpc.hpp"

#include <rxcpp/rx-observable.hpp>
#include "backend/protobuf/proposal.hpp"
#include "backend/protobuf/transaction.hpp"
#include "interfaces/common_objects/peer.hpp"
#include "interfaces/iroha_internal/transaction_batch.hpp"
#include "logger/logger.hpp"
#include "network/impl/client_factory.hpp"

using namespace iroha;
using namespace iroha::ordering;
using namespace iroha::ordering::transport;

OnDemandOsClientGrpc::OnDemandOsClientGrpc(
    std::shared_ptr<proto::OnDemandOrdering::StubInterface> stub,
    std::shared_ptr<network::AsyncGrpcClient> async_call,
    std::shared_ptr<TransportFactoryType> proposal_factory,
    std::function<TimepointType()> time_provider,
    std::chrono::milliseconds proposal_request_timeout,
    logger::LoggerPtr log)
    : log_(std::move(log)),
      stub_(std::move(stub)),
      async_call_(std::move(async_call)),
      proposal_factory_(std::move(proposal_factory)),
      time_provider_(std::move(time_provider)),
      proposal_request_timeout_(proposal_request_timeout) {}

void OnDemandOsClientGrpc::onBatches(CollectionType batches) {
  proto::BatchesRequest request;
  for (auto &batch : batches) {
    for (auto &transaction : batch->transactions()) {
      *request.add_transactions() = std::move(
          static_cast<shared_model::proto::Transaction *>(transaction.get())
              ->getTransport());
    }
  }

  log_->debug("Propagating: '{}'", request.DebugString());

  async_call_->Call(
      [&](auto context, auto cq) {
        return stub_->AsyncSendBatches(context, request, cq);
      },
      std::function<void(grpc::Status &, google::protobuf::Empty &)>{});
}

rxcpp::observable<
    boost::optional<std::shared_ptr<OdOsNotification::ProposalType const>>>
OnDemandOsClientGrpc::onRequestProposal(consensus::Round round) {
  return rxcpp::observable<>::create<
      boost::optional<std::shared_ptr<OdOsNotification::ProposalType const>>>(
      [round,
       stub_ = std::weak_ptr<proto::OnDemandOrdering::StubInterface>(stub_),
       async_call_ = std::weak_ptr<network::AsyncGrpcClient>(async_call_),
       proposal_factory_ =
           std::weak_ptr<TransportFactoryType>(proposal_factory_),
       time_provider = time_provider_,
       proposal_request_timeout = proposal_request_timeout_](auto s) {
        auto stub = stub_.lock();
        auto async_call = async_call_.lock();
        auto proposal_factory = proposal_factory_.lock();

        if (stub and async_call and proposal_factory) {
          async_call->Call(
              [round, stub, time_provider, proposal_request_timeout](
                  auto *context, auto *cq) {
                context->set_deadline(time_provider()
                                      + proposal_request_timeout);
                proto::ProposalRequest request;
                request.mutable_round()->set_block_round(round.block_round);
                request.mutable_round()->set_reject_round(round.reject_round);
                return stub->AsyncRequestProposal(context, request, cq);
              },
              std::function<void(grpc::Status &, proto::ProposalResponse &)>(
                  [s, proposal_factory](auto &status,
                                        proto::ProposalResponse &response) {
                    if (not status.ok()) {
                      // RPC failed
                      s.on_error(std::make_exception_ptr(
                          std::runtime_error(status.error_message())));
                      return;
                    }
                    if (not response.has_proposal()) {
                      s.on_next(boost::none);
                      s.on_completed();
                      return;
                    }
                    proposal_factory->build(response.proposal())
                        .match(
                            [s](auto &&v) {
                              s.on_next(boost::make_optional(
                                  std::shared_ptr<
                                      OdOsNotification::ProposalType const>(
                                      std::move(v).value)));
                              s.on_completed();
                            },
                            [s](auto &&error) {
                              s.on_error(
                                  std::make_exception_ptr(std::runtime_error(
                                      std::move(error).error.error)));
                            });
                  }));
        }
      });
}

OnDemandOsClientGrpcFactory::OnDemandOsClientGrpcFactory(
    std::shared_ptr<network::AsyncGrpcClient> async_call,
    std::shared_ptr<TransportFactoryType> proposal_factory,
    std::function<OnDemandOsClientGrpc::TimepointType()> time_provider,
    OnDemandOsClientGrpc::TimeoutType proposal_request_timeout,
    logger::LoggerPtr client_log,
    std::unique_ptr<ClientFactory> client_factory)
    : async_call_(std::move(async_call)),
      proposal_factory_(std::move(proposal_factory)),
      time_provider_(time_provider),
      proposal_request_timeout_(proposal_request_timeout),
      client_log_(std::move(client_log)),
      client_factory_(std::move(client_factory)) {}

expected::Result<std::unique_ptr<OdOsNotification>, std::string>
OnDemandOsClientGrpcFactory::create(const shared_model::interface::Peer &to) {
  return client_factory_->createClient(to) |
             [&](auto &&client) -> std::unique_ptr<OdOsNotification> {
    return std::make_unique<OnDemandOsClientGrpc>(std::move(client),
                                                  async_call_,
                                                  proposal_factory_,
                                                  time_provider_,
                                                  proposal_request_timeout_,
                                                  client_log_);
  };
}
