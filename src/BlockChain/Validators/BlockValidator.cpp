#include "BlockValidator.h"

#include <Crypto/Crypto.h>
#include <Core/Exceptions/BlockChainException.h>
#include <Core/Exceptions/BadDataException.h>
#include <Core/Validation/TransactionBodyValidator.h>
#include <Core/Validation/KernelSumValidator.h>
#include <Common/Util/FunctionalUtil.h>
#include <Consensus/Common.h>
#include <PMMR/TxHashSet.h>
#include <algorithm>

// Validates all the elements in a block that can be checked without additional data. 
// Includes commitment sums and kernels, reward, etc.
void BlockValidator::VerifySelfConsistent(const FullBlock& block)
{
	if (block.WasValidated())
	{
		LOG_TRACE_F("Block {} already validated", block);
		return;
	}

	// TODO - NRD: Verify kernel variants
	VerifyBody(block);
	VerifyKernelLockHeights(block);
	VerifyCoinbase(block);

	block.MarkAsValidated();
}

void BlockValidator::VerifyBody(const FullBlock& block)
{
	try
	{
		TransactionBodyValidator().Validate(block.GetTransactionBody(), false);
	}
	catch (std::exception& e)
	{
		LOG_ERROR_F("Transaction body for block {} failed with error: {}", block, e.what());
		throw BAD_DATA_EXCEPTION("Failed to validate transaction body");
	}
}

// check we have no kernels with lock_heights greater than current height
// no tx can be included in a block earlier than its lock_height
void BlockValidator::VerifyKernelLockHeights(const FullBlock& block)
{
	const uint64_t blockHeight = block.GetHeight();
	const std::vector<TransactionKernel>& kernels = block.GetKernels();
	const bool invalid = std::any_of(
		kernels.cbegin(),
		kernels.cend(),
		[blockHeight](const TransactionKernel& kernel) { return kernel.GetLockHeight() > blockHeight; }
	);
	if (invalid)
	{
		LOG_ERROR_F("Failed to validate kernel lock heights for {}", block);
		throw BAD_DATA_EXCEPTION("Failed to validate kernel lock heights");
	}
}

// Validate the coinbase outputs generated by miners.
// Check the sum of coinbase-marked outputs match the sum of coinbase-marked kernels accounting for fees.
void BlockValidator::VerifyCoinbase(const FullBlock& block)
{
	// Get Coinbase Output Commitments
	const std::vector<TransactionOutput>& blockOutputs = block.GetOutputs();
	std::vector<Commitment> coinbaseCommitments;
	FunctionalUtil::transform_if(
		blockOutputs.cbegin(),
		blockOutputs.cend(),
		std::back_inserter(coinbaseCommitments),
		[](const TransactionOutput& output) { return output.IsCoinbase(); },
		[](const TransactionOutput& output) { return output.GetCommitment(); }
	);

	// Get Coinbase Kernel Commitments
	const std::vector<TransactionKernel>& blockKernels = block.GetKernels();
	std::vector<Commitment> coinbaseKernelExcesses;
	FunctionalUtil::transform_if(
		blockKernels.cbegin(),
		blockKernels.cend(),
		std::back_inserter(coinbaseKernelExcesses),
		[](const TransactionKernel& kernel) { return kernel.IsCoinbase(); },
		[](const TransactionKernel& kernel) { return kernel.GetExcessCommitment(); }
	);

	// Calculate Block Reward
	const uint64_t reward = std::accumulate(
		blockKernels.cbegin(),
		blockKernels.cend(),
		Consensus::REWARD,
		[](uint64_t reward, const TransactionKernel& kernel) { return reward + kernel.GetFee(); }
	);

	// Verify the kernel sum equals the output sum accounting for block fees.
	const std::vector<Commitment> overCommitment({ Crypto::CommitTransparent(reward) });
	const Commitment outputAdjustedSum = Crypto::AddCommitments(coinbaseCommitments, overCommitment);
	const Commitment kernelSum = Crypto::AddCommitments(coinbaseKernelExcesses, {});

	if (kernelSum != outputAdjustedSum)
	{
		LOG_ERROR_F("Failed to validate coinbase for {}", block);
		throw BAD_DATA_EXCEPTION("Failed to validate coinbase");
	}
}