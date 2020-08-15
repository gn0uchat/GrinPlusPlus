#pragma once

#include <Crypto/SecretKey.h>
#include <Crypto/ProofMessage.h>
#include <cstdint>

class RewoundProof
{
public:
	RewoundProof(const uint64_t amount, std::unique_ptr<SecretKey>&& pBlindingFactor, ProofMessage&& proofMessage)
		: m_amount(amount), m_pBlindingFactor(std::move(pBlindingFactor)), m_proofMessage(std::move(proofMessage))
	{

	}

	uint64_t GetAmount() const noexcept { return m_amount; }
	const std::unique_ptr<SecretKey>& GetBlindingFactor() const noexcept { return m_pBlindingFactor; }
	const ProofMessage& GetProofMessage() const noexcept { return m_proofMessage; }

private:
	uint64_t m_amount;
	std::unique_ptr<SecretKey> m_pBlindingFactor;
	ProofMessage m_proofMessage;
};