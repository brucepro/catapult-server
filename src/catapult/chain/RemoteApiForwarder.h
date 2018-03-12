#pragma once
#include "NodeInteractionResult.h"
#include "catapult/model/TransactionPlugin.h"
#include "catapult/net/PacketIoPicker.h"
#include "catapult/thread/Future.h"
#include "catapult/utils/MemoryUtils.h"
#include "catapult/utils/ThrottleLogger.h"
#include "catapult/utils/TimeSpan.h"

namespace catapult { namespace chain {

	/// Simplifies interacting with remote nodes via apis.
	class RemoteApiForwarder {
	public:
		/// Creates a forwarder around a peer selector (\a packetIoPicker) with a connection \a timeout
		/// given a transaction registry (\a transactionRegistry) and a friendly name (\a operationName).
		RemoteApiForwarder(
				net::PacketIoPicker& packetIoPicker,
				const model::TransactionRegistry& transactionRegistry,
				const utils::TimeSpan& timeout,
				const std::string& operationName)
				: m_packetIoPicker(packetIoPicker)
				, m_transactionRegistry(transactionRegistry)
				, m_timeout(timeout)
				, m_operationName(operationName)
		{}

	public:
		/// Picks a random peer, wraps an api around it using \a apiFactory, and then passes the api to \a action.
		template<typename TRemoteApiAction, typename TRemoteApiFactory>
		thread::future<NodeInteractionResult> processSync(TRemoteApiAction action, TRemoteApiFactory apiFactory) const {
			auto packetIoPair = m_packetIoPicker.pickOne(m_timeout);
			if (!packetIoPair) {
				CATAPULT_LOG_THROTTLE(warning, 60'000) << "no packet io available for operation '" << m_operationName << "'";
				return thread::make_ready_future(NodeInteractionResult::None);
			}

			// pass in a non-owning pointer to the registry
			auto pRemoteApi = utils::UniqueToShared(apiFactory(*packetIoPair.io(), m_transactionRegistry));

			// extend the lifetimes of pRemoteApi and packetIoPair until the completion of the action
			// (pRemoteApi is a pointer so that the reference taken by action is valid throughout the entire asynchronous action)
			return action(*pRemoteApi).then([pRemoteApi, packetIoPair, operationName = m_operationName](auto&& resultFuture) {
				auto result = resultFuture.get();
				CATAPULT_LOG_LEVEL(NodeInteractionResult::Neutral == result ? utils::LogLevel::Trace : utils::LogLevel::Info)
						<< "completed '" << operationName << "' (" << packetIoPair.node() << ") with result " << result;
				return result;
			});
		}

	private:
		net::PacketIoPicker& m_packetIoPicker;
		const model::TransactionRegistry& m_transactionRegistry;
		utils::TimeSpan m_timeout;
		std::string m_operationName;
	};
}}