#include "BlockValidator.h"
#include "../Processors/BlockHeaderProcessor.h"

#include <Crypto.h>
#include <Consensus/Common.h>
#include <PMMR/TxHashSet.h>
#include <TxPool/TransactionPool.h>

BlockValidator::BlockValidator(const ITransactionPool& transactionPool, const ITxHashSet* pTxHashSet)
	: m_transactionPool(transactionPool), m_pTxHashSet(pTxHashSet)
{

}

// Validates a block is self-consistent and validates the state (eg. MMRs).
bool BlockValidator::IsBlockValid(const FullBlock& block, const BlindingFactor& previousKernelOffset, const bool validateSelfConsistent) const
{
	if (validateSelfConsistent)
	{
		if (!IsBlockSelfConsistent(block))
		{
			return false;
		}
	}

	BlindingFactor blockKernelOffset(CBigInteger<32>::ValueOf(0));

	// take the kernel offset for this block (block offset minus previous) and verify.body.outputs and kernel sums
	if (block.GetBlockHeader().GetTotalKernelOffset() == previousKernelOffset)
	{
		std::unique_ptr<BlindingFactor> pBlockKernelOffset = Crypto::AddBlindingFactors(std::vector<BlindingFactor>({ block.GetBlockHeader().GetTotalKernelOffset() }), std::vector<BlindingFactor>({ previousKernelOffset }));
		if (pBlockKernelOffset == nullptr)
		{
			return false;
		}

		blockKernelOffset = *pBlockKernelOffset;
	}

	const bool kernelSumsValid = VerifyKernelSums(block, 0 - Consensus::REWARD, blockKernelOffset);
	if (!kernelSumsValid)
	{
		return false;
	}

	// TODO: Validate MMRs

	return true;
}

// Validates all the elements in a block that can be checked without additional data. 
// Includes commitment sums and kernels, reward, etc.
bool BlockValidator::IsBlockSelfConsistent(const FullBlock& block) const
{
	if (!m_transactionPool.ValidateTransactionBody(block.GetTransactionBody(), true))
	{
		return false;
	}

	if (!VerifyKernelLockHeights(block))
	{
		return false;
	}

	if (!VerifyCoinbase(block))
	{
		return false;
	}

	return true;
}

// check we have no kernels with lock_heights greater than current height
// no tx can be included in a block earlier than its lock_height
bool BlockValidator::VerifyKernelLockHeights(const FullBlock& block) const
{
	const uint64_t height = block.GetBlockHeader().GetHeight();
	for (const TransactionKernel& kernel : block.GetTransactionBody().GetKernels())
	{
		if (kernel.GetLockHeight() > height)
		{
			return false;
		}
	}

	return true;
}

// Validate the coinbase outputs generated by miners.
// Check the sum of coinbase-marked outputs match the sum of coinbase-marked kernels accounting for fees.
bool BlockValidator::VerifyCoinbase(const FullBlock& block) const
{
	std::vector<Commitment> coinbaseCommitments;
	for (const TransactionOutput& output : block.GetTransactionBody().GetOutputs())
	{
		if ((output.GetFeatures() & EOutputFeatures::COINBASE_OUTPUT) == EOutputFeatures::COINBASE_OUTPUT)
		{
			coinbaseCommitments.push_back(output.GetCommitment());
		}
	}

	std::vector<Commitment> coinbaseKernelExcesses;
	for (const TransactionKernel& kernel : block.GetTransactionBody().GetKernels())
	{
		if ((kernel.GetFeatures() & EKernelFeatures::COINBASE_KERNEL) == EKernelFeatures::COINBASE_KERNEL)
		{
			coinbaseKernelExcesses.push_back(kernel.GetExcessCommitment());
		}
	}

	uint64_t reward = Consensus::REWARD;
	for (const TransactionKernel& kernel : block.GetTransactionBody().GetKernels())
	{
		reward += kernel.GetFee();
	}

	std::unique_ptr<Commitment> pRewardCommitment = Crypto::CommitTransparent(reward);
	if (pRewardCommitment == nullptr)
	{
		return false;
	}

	const std::vector<Commitment> overCommitment({ *pRewardCommitment });
	const std::unique_ptr<Commitment> pOutputAdjustedSum = Crypto::AddCommitments(coinbaseCommitments, overCommitment);

	const std::unique_ptr<Commitment> pKernelSum = Crypto::AddCommitments(coinbaseKernelExcesses, std::vector<Commitment>());

	// Verify the kernel sum equals the output sum accounting for block fees.
	if (pOutputAdjustedSum == nullptr || pKernelSum == nullptr)
	{
		return false;
	}

	return *pKernelSum == *pOutputAdjustedSum;
}

bool BlockValidator::VerifyKernelSums(const FullBlock& block, int64_t overage, const BlindingFactor& kernelOffset) const
{
	// TODO: Implement
	return true;
}