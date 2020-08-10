#include "BlockDBImpl.h"
#include "RocksDB/RocksDBFactory.h"
#include "RocksDB/RocksDB.h"

#include <Database/DatabaseException.h>
#include <Common/Logger.h>
#include <Common/Util/StringUtil.h>
#include <utility>
#include <string>
#include <filesystem.h>

using namespace rocksdb;

std::shared_ptr<BlockDB> BlockDB::OpenDB(const Config& config)
{
	fs::path dbPath = config.GetNodeConfig().GetDatabasePath() / "CHAIN/";

	ColumnFamilyDescriptor BLOCK_COLUMN = ColumnFamilyDescriptor("BLOCK", *ColumnFamilyOptions().OptimizeForPointLookup(1024));
	ColumnFamilyDescriptor HEADER_COLUMN = ColumnFamilyDescriptor("HEADER", *ColumnFamilyOptions().OptimizeForPointLookup(1024));
	ColumnFamilyDescriptor BLOCK_SUMS_COLUMN = ColumnFamilyDescriptor("BLOCK_SUMS", *ColumnFamilyOptions().OptimizeForPointLookup(1024));
	ColumnFamilyDescriptor OUTPUT_POS_COLUMN = ColumnFamilyDescriptor("OUTPUT_POS", *ColumnFamilyOptions().OptimizeForPointLookup(1024));
	ColumnFamilyDescriptor INPUT_BITMAP_COLUMN = ColumnFamilyDescriptor("INPUT_BITMAP", *ColumnFamilyOptions().OptimizeForPointLookup(1024));
	ColumnFamilyDescriptor SPENT_OUTPUTS_COLUMN = ColumnFamilyDescriptor("SPENT_OUTPUTS", *ColumnFamilyOptions().OptimizeForPointLookup(1024));

	std::vector<ColumnFamilyDescriptor> tableNames = { ColumnFamilyDescriptor(), BLOCK_COLUMN, HEADER_COLUMN, BLOCK_SUMS_COLUMN, OUTPUT_POS_COLUMN, INPUT_BITMAP_COLUMN, SPENT_OUTPUTS_COLUMN };
	std::shared_ptr<RocksDB> pRocksDB = RocksDBFactory::Open(dbPath, tableNames);
	pRocksDB->DeleteAll("INPUT_BITMAP");

	return std::make_shared<BlockDB>(config, pRocksDB);
}

void BlockDB::Commit()
{
	m_pRocksDB->Commit();

	for (auto pHeader : m_uncommitted)
	{
		m_blockHeadersCache.Put(pHeader->GetHash(), pHeader);
	}

	m_uncommitted.clear();
}

void BlockDB::Rollback() noexcept
{
	m_uncommitted.clear();
	m_pRocksDB->Rollback();
}

BlockHeaderPtr BlockDB::GetBlockHeader(const Hash& hash) const
{
	if (m_blockHeadersCache.Cached(hash))
	{
		return m_blockHeadersCache.Get(hash);
	}

	rocksdb::Slice key((const char*)hash.data(), hash.size());
	auto pBlockHeader = m_pRocksDB->Get<BlockHeader>("HEADER", key);
	if (pBlockHeader != nullptr)
	{
		return std::shared_ptr<BlockHeader>(std::move(pBlockHeader));
	}

	return nullptr;
}

void BlockDB::AddBlockHeader(BlockHeaderPtr pBlockHeader)
{
	LOG_TRACE_F("Adding header {}", *pBlockHeader);

	const Hash& hash = pBlockHeader->GetHash();
	rocksdb::Slice key((const char*)hash.data(), hash.size());
	m_pRocksDB->Put("HEADER", DBEntry<BlockHeader>(key, *pBlockHeader));

	if (m_pRocksDB->IsTransactional())
	{
		m_uncommitted.push_back(pBlockHeader);
	}
	else
	{
		m_blockHeadersCache.Put(pBlockHeader->GetHash(), pBlockHeader);
	}
}

void BlockDB::AddBlockHeaders(const std::vector<BlockHeaderPtr>& blockHeaders)
{
	LOG_TRACE_F("Adding {} headers.", blockHeaders.size());

	std::vector<DBEntry<BlockHeader>> entries;
	entries.reserve(blockHeaders.size());

	for (auto pBlockHeader : blockHeaders)
	{
		const Hash& hash = pBlockHeader->GetHash();
		rocksdb::Slice key((const char*)hash.data(), hash.size());
		entries.push_back({ key, *pBlockHeader });
	}

	m_pRocksDB->Put("HEADER", entries);

	LOG_TRACE("Finished adding headers.");
}

void BlockDB::AddBlock(const FullBlock& block)
{
	LOG_TRACE_F("Adding block {}", block);

	const std::vector<unsigned char>& hash = block.GetHash().GetData();
	rocksdb::Slice key((const char*)hash.data(), hash.size());
	m_pRocksDB->Put("BLOCK", DBEntry<FullBlock>(key, block));
}

std::unique_ptr<FullBlock> BlockDB::GetBlock(const Hash& hash) const
{
	rocksdb::Slice key((const char*)hash.data(), hash.size());
	return m_pRocksDB->Get<FullBlock>("BLOCK", key);
}

void BlockDB::ClearBlocks()
{
	LOG_WARNING("Deleting all blocks.");

	m_pRocksDB->DeleteAll("BLOCK");
}

void BlockDB::AddBlockSums(const Hash& blockHash, const BlockSums& blockSums)
{
	LOG_TRACE_F("Adding BlockSums for block {}", blockHash);

	rocksdb::Slice key((const char*)blockHash.data(), blockHash.size());
	 m_pRocksDB->Put("BLOCK_SUMS", DBEntry<BlockSums>(key, blockSums));
}

std::unique_ptr<BlockSums> BlockDB::GetBlockSums(const Hash& blockHash) const
{
	rocksdb::Slice key((const char*)blockHash.data(), blockHash.size());
	return m_pRocksDB->Get<BlockSums>("BLOCK_SUMS", key);
}

void BlockDB::ClearBlockSums()
{
	LOG_WARNING("Deleting all block sums.");

	m_pRocksDB->DeleteAll("BLOCK_SUMS");
}

void BlockDB::AddOutputPosition(const Commitment& outputCommitment, const OutputLocation& location)
{
	rocksdb::Slice key((const char*)outputCommitment.data(), outputCommitment.size());

	m_pRocksDB->Put("OUTPUT_POS", DBEntry<OutputLocation>(key, location));
}

std::unique_ptr<OutputLocation> BlockDB::GetOutputPosition(const Commitment& outputCommitment) const
{
	rocksdb::Slice key((const char*)outputCommitment.data(), outputCommitment.size());
	return m_pRocksDB->Get<OutputLocation>("OUTPUT_POS", key);
}

void BlockDB::RemoveOutputPositions(const std::vector<Commitment>& outputCommitments)
{
	std::vector<std::string> keys;
	std::transform(
		outputCommitments.begin(), outputCommitments.end(),
		std::back_inserter(keys),
		[](const Commitment& commit) { return std::string((const char*)commit.data(), commit.size()); }
	);

	m_pRocksDB->Delete("OUTPUT_POS", keys);
}

void BlockDB::ClearOutputPositions()
{
	LOG_WARNING("Deleting all output positions.");

	m_pRocksDB->DeleteAll("OUTPUT_POS");
}

void BlockDB::AddSpentPositions(const Hash& blockHash, const std::vector<SpentOutput>& outputPositions)
{
	assert(outputPositions.size() < (size_t)UINT16_MAX);

	rocksdb::Slice key((const char*)blockHash.data(), blockHash.size());
	m_pRocksDB->Put("SPENT_OUTPUTS", DBEntry<SpentOutputs>(key, std::make_unique<SpentOutputs>(outputPositions)));
}

std::unordered_map<Commitment, OutputLocation> BlockDB::GetSpentPositions(const Hash& blockHash) const
{
	rocksdb::Slice key((const char*)blockHash.data(), blockHash.size());

	auto pSpent = m_pRocksDB->Get<SpentOutputs>("SPENT_OUTPUTS", key);
	if (pSpent == nullptr)
	{
		LOG_ERROR_F("Failed to retrieve spent positions for block ({})", blockHash);
		throw DATABASE_EXCEPTION("Failed to retrieve spent positions");
	}

	return pSpent->BuildMap();
}

void BlockDB::ClearSpentPositions()
{
	LOG_WARNING("Deleting all spent positions.");

	m_pRocksDB->DeleteAll("SPENT_OUTPUTS");
}

void BlockDB::OnInitWrite()
{
	m_pRocksDB->OnInitWrite();
}

void BlockDB::OnEndWrite()
{
	m_pRocksDB->OnEndWrite();
}