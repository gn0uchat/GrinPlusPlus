#pragma once

// Copyright (c) 2018-2019 David Burkett
// Distributed under the MIT software license, see the accompanying
// file LICENSE or http://www.opensource.org/licenses/mit-license.php.

#include <vector>
#include <Core/Models/BlockHeader.h>
#include <Core/Models/TransactionBody.h>
#include <Core/Serialization/ByteBuffer.h>
#include <Core/Serialization/Serializer.h>
#include <Core/Traits/Printable.h>
#include <json/json.h>

class FullBlock : public Traits::IPrintable
{
public:
	//
	// Constructors
	//
	FullBlock(BlockHeaderPtr pBlockHeader, TransactionBody&& transactionBody);
	FullBlock(const FullBlock& other) = default;
	FullBlock(FullBlock&& other) noexcept = default;
	FullBlock() = default;

	//
	// Destructor
	//
	virtual ~FullBlock() = default;

	//
	// Operators
	//
	FullBlock& operator=(const FullBlock& other) = default;
	FullBlock& operator=(FullBlock&& other) noexcept = default;

	//
	// Getters
	//
	const BlockHeaderPtr& GetBlockHeader() const noexcept { return m_pBlockHeader; }
	const TransactionBody& GetTransactionBody() const noexcept { return m_transactionBody; }

	const std::vector<TransactionInput>& GetInputs() const noexcept { return m_transactionBody.GetInputs(); }
	const std::vector<TransactionOutput>& GetOutputs() const noexcept { return m_transactionBody.GetOutputs(); }
	const std::vector<TransactionKernel>& GetKernels() const noexcept { return m_transactionBody.GetKernels(); }

	std::vector<Commitment> GetInputCommitments() const
	{
		const auto& inputs = GetInputs();

		std::vector<Commitment> commitments;
		commitments.reserve(inputs.size());

		std::transform(
			inputs.cbegin(), inputs.cend(),
			std::back_inserter(commitments),
			[](const TransactionInput& input) { return input.GetCommitment(); }
		);

		return commitments;
	}

	std::vector<Commitment> GetOutputCommitments() const
	{
		const auto& outputs = GetOutputs();

		std::vector<Commitment> commitments;
		commitments.reserve(outputs.size());

		std::transform(
			outputs.cbegin(), outputs.cend(),
			std::back_inserter(commitments),
			[](const TransactionOutput& output) { return output.GetCommitment(); }
		);

		return commitments;
	}

	uint64_t GetHeight() const noexcept { return m_pBlockHeader->GetHeight(); }
	const Hash& GetPreviousHash() const noexcept { return m_pBlockHeader->GetPreviousBlockHash(); }
	uint64_t GetTotalDifficulty() const noexcept { return m_pBlockHeader->GetTotalDifficulty(); }
	const BlindingFactor& GetTotalKernelOffset() const noexcept { return m_pBlockHeader->GetTotalKernelOffset(); }


	//
	// Serialization/Deserialization
	//
	void Serialize(Serializer& serializer) const;
	static FullBlock Deserialize(ByteBuffer& byteBuffer);
	Json::Value ToJSON() const;

	//
	// Hashing
	//
	const Hash& GetHash() const noexcept { return m_pBlockHeader->GetHash(); }

	//
	// Validation Status
	//
	bool WasValidated() const noexcept { return m_validated; }
	void MarkAsValidated() const noexcept { m_validated = true; }

	//
	// Traits
	//
	virtual std::string Format() const override final { return GetHash().ToHex(); }

private:
	BlockHeaderPtr m_pBlockHeader;
	TransactionBody m_transactionBody;
	mutable bool m_validated;
};