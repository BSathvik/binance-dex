// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <txdb.h>
#include <core_io.h>

#include <chainparams.h>
#include <hash.h>
#include <random.h>
#include <pow.h>
#include <uint256.h>
#include <util.h>
#include <ui_interface.h>
#include <init.h>
#include <validation.h>
#include <univalue.h>
#include <stdint.h>
#include <uint256.h>
#include <numeric>
#include <index/txindex.h>

#include <boost/algorithm/string.hpp>
#include <boost/thread.hpp>

static const char DB_COIN = 'C';
static const char DB_COINS = 'c';
static const char DB_BLOCK_FILES = 'f';
static const char DB_TXINDEX = 't';
static const char DB_TXINDEX_BLOCK = 'T';
static const char DB_BLOCK_INDEX = 'b';

static const char DB_BEST_BLOCK = 'B';
static const char DB_HEAD_BLOCKS = 'H';
static const char DB_FLAG = 'F';
static const char DB_REINDEX_FLAG = 'R';
static const char DB_LAST_BLOCK = 'l';

/* DB_VOTE_COUNT: Entry in LevelDB prefixed by 'v'
 * key: an address
 * value: number of votes
 */

static const char DB_VOTE_COUNT = 'v';

/* DB_ADDR_CANDIDATES: Entry in LevelDB prefixed by 'V'
 * key: a voting address
 * value: a list of candidates (addresses) this account has voted for
 */

static const char DB_ADDR_CANDIDATES = 'V';

// These next prefixes are just to make it easy

/* DB_CANDIDATE_ADDRS: Entry in LevelDB prefixed by 'a'
 * key: an enrolled address
 * value: a list of addresses that are voting for it
 */

static const char DB_CANDIDATES_ADDR = 'a';

// need this because it otherwise you need to scan the entire blockchain for balances, make cleaner and more secure

/* DB_ADDR_BAL: Entry in LevelDB prefixed by 'b'
 * key: an address
 * value: that addresses' balance
 */

static const char DB_ADDR_BAL = 'A';

/* DB_ASSET_FROZEN: Entry in LevelDB prefixed by 'F'
 * key: an asset type
 * value: 0 if not frozen, 1 if frozen
 */

static const char DB_ASSET_FROZEN = 'F';

namespace {

struct CoinEntry {
    COutPoint* outpoint;
    char key;
    explicit CoinEntry(const COutPoint* ptr) : outpoint(const_cast<COutPoint*>(ptr)), key(DB_COIN)  {}

    template<typename Stream>
    void Serialize(Stream &s) const {
        s << key;
        s << outpoint->hash;
        s << VARINT(outpoint->n);
    }

    template<typename Stream>
    void Unserialize(Stream& s) {
        s >> key;
        s >> outpoint->hash;
        s >> VARINT(outpoint->n);
    }
};

}

CCoinsViewDB::CCoinsViewDB(size_t nCacheSize, bool fMemory, bool fWipe) : db(GetDataDir() / "chainstate", nCacheSize,
                                                                             fMemory, fWipe, true)
{
}

bool CCoinsViewDB::GetCoin(const COutPoint &outpoint, Coin &coin) const {
    return db.Read(CoinEntry(&outpoint), coin);
}

bool CCoinsViewDB::HaveCoin(const COutPoint &outpoint) const {
    return db.Exists(CoinEntry(&outpoint));
}

uint256 CCoinsViewDB::GetBestBlock() const {
    uint256 hashBestChain;
    if (!db.Read(DB_BEST_BLOCK, hashBestChain))
        return uint256();
    return hashBestChain;
}

std::vector<uint256> CCoinsViewDB::GetHeadBlocks() const {
    std::vector<uint256> vhashHeadBlocks;
    if (!db.Read(DB_HEAD_BLOCKS, vhashHeadBlocks)) {
        return std::vector<uint256>();
    }
    return vhashHeadBlocks;
}

bool CCoinsViewDB::BatchWrite(CCoinsMap &mapCoins, const uint256 &hashBlock) {
    CDBBatch batch(db);
    size_t count = 0;
    size_t changed = 0;
    size_t batch_size = (size_t)gArgs.GetArg("-dbbatchsize", nDefaultDbBatchSize);
    int crash_simulate = gArgs.GetArg("-dbcrashratio", 0);
    assert(!hashBlock.IsNull());

    uint256 old_tip = GetBestBlock();
    if (old_tip.IsNull()) {
        // We may be in the middle of replaying.
        std::vector<uint256> old_heads = GetHeadBlocks();
        if (old_heads.size() == 2) {
            assert(old_heads[0] == hashBlock);
            old_tip = old_heads[1];
        }
    }

    // In the first batch, mark the database as being in the middle of a
    // transition from old_tip to hashBlock.
    // A vector is used for future extensibility, as we may want to support
    // interrupting after partial writes from multiple independent reorgs.
    batch.Erase(DB_BEST_BLOCK);
    batch.Write(DB_HEAD_BLOCKS, std::vector<uint256>{hashBlock, old_tip});

    for (CCoinsMap::iterator it = mapCoins.begin(); it != mapCoins.end();) {
        if (it->second.flags & CCoinsCacheEntry::DIRTY) {
            CoinEntry entry(&it->first);
            if (it->second.coin.IsSpent())
                batch.Erase(entry);
            else
                batch.Write(entry, it->second.coin);
            changed++;
        }
        count++;
        CCoinsMap::iterator itOld = it++;
        mapCoins.erase(itOld);
        if (batch.SizeEstimate() > batch_size) {
            LogPrint(BCLog::COINDB, "Writing partial batch of %.2f MiB\n", batch.SizeEstimate() * (1.0 / 1048576.0));
            db.WriteBatch(batch);
            batch.Clear();
            if (crash_simulate) {
                static FastRandomContext rng;
                if (rng.randrange(crash_simulate) == 0) {
                    LogPrintf("Simulating a crash. Goodbye.\n");
                    _Exit(0);
                }
            }
        }
    }

    // In the last batch, mark the database as consistent with hashBlock again.
    batch.Erase(DB_HEAD_BLOCKS);
    batch.Write(DB_BEST_BLOCK, hashBlock);

    LogPrint(BCLog::COINDB, "Writing final batch of %.2f MiB\n", batch.SizeEstimate() * (1.0 / 1048576.0));
    bool ret = db.WriteBatch(batch);
    LogPrint(BCLog::COINDB, "Committed %u changed transaction outputs (out of %u) to coin database...\n", (unsigned int)changed, (unsigned int)count);
    return ret;
}

size_t CCoinsViewDB::EstimateSize() const
{
    return db.EstimateSize(DB_COIN, (char)(DB_COIN+1));
}

CBlockTreeDB::CBlockTreeDB(size_t nCacheSize, bool fMemory, bool fWipe) : CDBWrapper(gArgs.IsArgSet("-blocksdir") ? GetDataDir() / "blocks" / "index" : GetBlocksDir() / "index", nCacheSize, fMemory, fWipe) {
}

// TODO: Make WriteAddrCandidates and WriteCandidatesAddr instead of creating strings and splitting,
// just appending to keys and iterating through keys, for example:
// v<ADDRESS>0: address
// v<ADDRESS>1: another address
// v<ADDRESS>2: another address

bool CBlockTreeDB::WriteAddrCandidates(const std::string addr, std::vector<std::string> enrolled) {
  if(enrolled.size()==0)
    return true;

  std::vector<std::string> tempEnrolled;
  if(ReadCandidatesAddr(addr, tempEnrolled))
    enrolled.insert(enrolled.end(), tempEnrolled.begin(), tempEnrolled.end());

  std::ostringstream ss;
  std::copy(enrolled.begin(), enrolled.end()-1, std::ostream_iterator<std::string>(ss, ","));
  ss << enrolled.back();

  return Write(std::make_pair(DB_ADDR_CANDIDATES, addr), ss.str());
}

bool CBlockTreeDB::ReadAddrCandidates(const std::string addr, std::vector<std::string>& enrolled) {
  std::string str;
  if(!Read(std::make_pair(DB_ADDR_CANDIDATES, addr), str))
    return false;
  boost::split(enrolled, str, boost::is_any_of(","));
  return true;
}

bool CBlockTreeDB::WriteCandidatesAddr(const std::string addr, std::vector<std::string> voters) {
  if(voters.size()==0)
    return true;

  std::vector<std::string> tempVoters;
  if(ReadCandidatesAddr(addr, tempVoters))
    voters.insert(voters.end(), tempVoters.begin(), tempVoters.end());

  std::ostringstream ss;
  std::copy(voters.begin(), voters.end()-1, std::ostream_iterator<std::string>(ss, ","));
  ss << voters.back();

  return Write(std::make_pair(DB_CANDIDATES_ADDR, addr), ss.str());

}

bool CBlockTreeDB::ReadCandidatesAddr(const std::string addr, std::vector<std::string>& voters) {
  std::string str;
    if(!Read(std::make_pair(DB_CANDIDATES_ADDR, addr), str))
      return false;
    boost::split(voters, str, boost::is_any_of(","));
    return true;
}

bool CBlockTreeDB::IsEnrolled(const std::string addr) {
  int nVotes = 0;
  if(!ReadVoteCount(addr, nVotes))
    return false;
  return nVotes != -1;
}

bool CBlockTreeDB::IsAssetFrozen(const std::string assetType) {
  bool isFrozen;
  if(!ReadAssetFrozen(assetType, isFrozen))
    return false;
  return isFrozen;
}

bool CBlockTreeDB::WriteAssetFrozen(const std::string assetType, bool isFrozen) {
  int frozen = 0;
  if (isFrozen) frozen = 1;
  return Write(std::make_pair(DB_ASSET_FROZEN, assetType), frozen);
}

bool CBlockTreeDB::ReadAssetFrozen(const std::string assetType, bool& isFrozen) {
  int frozen = 0;
  if(!Read(std::make_pair(DB_ASSET_FROZEN, assetType), frozen))
    return false;
  isFrozen = frozen == 1;
  return true;
}

bool CBlockTreeDB::WriteAddrBalance(const std::string addr, int nAmount) {
  return Write(std::make_pair(DB_ADDR_BAL, addr), nAmount);
}

bool CBlockTreeDB::ReadAddrBalance(const std::string addr, int& nAmount) {
  return Read(std::make_pair(DB_ADDR_BAL, addr), nAmount);
}

bool CBlockTreeDB::ReadVoteCount(const std::string& addr, int nVotes) {
  bool success = Read(std::make_pair(DB_VOTE_COUNT, addr), nVotes);
  std::cout << "ReadVoteCount nVotes: " << nVotes << std::endl;
  return success;
}

/// TODO: Implement diff-wise vote counting at some point.
/// TODO: Confirm that read and write methods are correct
/// TODO: Confirm that the way the read and write methods are called are correct
/// TODO: Use a batch where appropriate
/// TODO: Test for edge cases and stuff
bool CBlockTreeDB::WriteVoteCount(const CBlock* block) {

  /// Add the coinbase value of each block to the database

  UniValue coinbaseCheckEntry(UniValue::VOBJ);
  TxToUniv(*(block->vtx.front()), (block->GetBlockHeader()).GetHash(), coinbaseCheckEntry);

  std::string coinbaseAddr = coinbaseCheckEntry["vout"][0]["scriptPubKey"]["addresses"][0].get_str();
  std::cout << "Received Address to send coinbase to: " << coinbaseAddr << std::endl;
  uint64_t coinbase = coinbaseCheckEntry["vout"][0]["value"].get_real();
  std::cout << "Received Coinbase amount: " << coinbase << std::endl;

  int nVoteCoinbase = 0;
  ReadVoteCount(coinbaseAddr, nVoteCoinbase);
  if(coinbase > 0) {
    Write(std::make_pair(DB_VOTE_COUNT, coinbaseAddr), nVoteCoinbase + coinbase);
  }

  int nBal = 0;
  /// if the output address to the coinbase transaction is not one of the top 10 then make writevotecount fail


  ReadAddrBalance(coinbaseAddr, nBal);
  WriteAddrBalance(coinbaseAddr, coinbase + nBal);

  for (std::vector< CTransactionRef >::const_iterator it = (block->vtx).begin(); it != (block->vtx).end(); ++it) {
    std::shared_ptr<const CTransaction> currTransaction = *it;

    if(currTransaction->type == CTransactionTypes::ENROLL) {
      int nVotes;

      UniValue entry(UniValue::VOBJ);
      TxToUniv(**it, (block->GetBlockHeader()).GetHash(), entry);

      /**
      * If there is no entry in the database associated to that address, OR
      * There is a non-even value for the amount of enrollments
      * This includes a value of -1, which is an arbitrary value indicating "not enrolled",
      * For addresses that have been enrolled once but have unenrolled
      */

      std::string sAddr = entry["vin"][0]["scriptSig"]["asm"].get_str();
      std::cout << "Retrieved Address: " << sAddr << std::endl;

      if(!ReadVoteCount(sAddr, nVotes) || nVotes==-1) {

        Write(std::make_pair(DB_VOTE_COUNT, sAddr), 0);

      } else {

        Write(std::make_pair(DB_VOTE_COUNT, sAddr), -1);

        nVotes = 0;
        int nAmount;

        if(!ReadAddrBalance(sAddr, nAmount)) {

          WriteAddrBalance(sAddr, 0);
          nAmount = 0;

        } else {

          std::vector<std::string> votingFor;
            if (!ReadCandidatesAddr(sAddr, votingFor))
                WriteCandidatesAddr(sAddr, votingFor);

            for(std::vector<std::string>::const_iterator iter = votingFor.begin(); iter != votingFor.end(); ++iter) {

            /// Loop through all the voters for this address, and process their databases

            std::string voter = *iter;
            std::vector<std::string> candidates;
            if(ReadAddrCandidates(voter, candidates)) {

              /// If reading candidates was successful, remove this candidate from this voter's DB_ADDR_CANDIDATES list

              candidates.erase(std::remove(candidates.begin(), candidates.end(), sAddr), candidates.end());
              WriteAddrCandidates(voter, candidates);

              for(std::vector<std::string>::const_iterator ii = candidates.begin(); ii != candidates.end(); ++ii) {

                /// For each other candidate increase their amount by some fraction of the voter's balance

                std::string otherCandidate = *ii;
                int voterBal = 0;

                if(!ReadAddrBalance(voter, voterBal))
                  WriteAddrBalance(voter, voterBal);

                int otherCandidateVotes = 0;
                if(!ReadVoteCount(otherCandidate, otherCandidateVotes))
                  Write(std::make_pair(DB_VOTE_COUNT, otherCandidate), (uint64_t)(voterBal/(candidates.size()+1)));

                Write(std::make_pair(DB_VOTE_COUNT, otherCandidate), (uint64_t)(otherCandidateVotes - voterBal/(candidates.size()+1) + voterBal/(candidates.size())));

              }

            }


          }

          std::vector<std::string> emptyVec;
          /// Let's see how it goes if I call this...
          WriteCandidatesAddr(sAddr, emptyVec);

          /**
           * For each address that is voting for this address:
           * Update their entries in the databases (this means DB_VOTE_COUNT, DB_CANDIDATES_ADDR, DB_ADDR_CANDIDATES)
           * This means using balance to increase vote count for all other candidates that are voted for by nodes voting for this address
           * And removing from x user's "voting for" databases
           */
        }

        /**
         * General loop logic for this method:
         * Take each address that voted for this address
         * Get their balance, increase other votes by an amount based on the amount of addresses they've voted on
         * Remove this address from the list
         * Set the value for this addresses' votes to -1
         */

      }

    } else if (currTransaction->type == CTransactionTypes::VOTE) {

      UniValue entry(UniValue::VOBJ);
      TxToUniv(**it, (block->GetBlockHeader()).GetHash(), entry);

      /**
      * Get both addresses, if the address has been voted for,
      * remove it from the list and update the other votes
      * If the address has not been voted for, add it to the list and update the votes
      */

      std::string inputAddr = entry["vin"][0]["scriptSig"]["asm"].get_str();
      std::cout << "Received input address: " << inputAddr << std::endl;
      std::string outputAddr;
      std::vector<UniValue> voutAddresses = entry["vout"][0]["scriptPubKey"]["addresses"].getValues();
      std::cout << "Received vout Addresses, here is the first: " << voutAddresses[0].get_str() << std::endl;
      if (voutAddresses.size() != 2)
        continue; /// Skip over this transaction, do not add because bad

      /// TODO: Make sure that this is the correct way to get the output address, make sure there can only be 2 unique output addresses
      for (unsigned int i = 0; i < voutAddresses.size(); i++)
        if (voutAddresses[i].get_str().compare(inputAddr) != 0)
          outputAddr = voutAddresses[i].get_str();

      std::vector<std::string> otherEnrolled;

      int nAmount = 0;
      ReadAddrBalance(inputAddr, nAmount);
      WriteAddrBalance(inputAddr, nAmount);

      /// If the key isn't found / they've never voted
      if (!ReadAddrCandidates(inputAddr, otherEnrolled)) {
        otherEnrolled.insert(otherEnrolled.end(), outputAddr);
        WriteAddrCandidates(inputAddr, otherEnrolled);

        /// In case entry doesn't exist
        std::vector<std::string> voters;
        ReadCandidatesAddr(outputAddr, voters);
        voters.insert(voters.end(), inputAddr);
        WriteCandidatesAddr(outputAddr, voters);

        /// In case it doesn't exist

        Write(std::make_pair(DB_VOTE_COUNT, outputAddr), nAmount);

      } else {
        /// If the key is found
        /// If it is found in this candidates' readaddrcandidates then unvote, otherwise vote
        if (std::find(otherEnrolled.begin(), otherEnrolled.end(), outputAddr) != otherEnrolled.end()) {
          /// Unvote (this address is un-voting and not voting)

          /// Erase from this voters' DB_ADDR_CANDIDATES list
          otherEnrolled.erase(std::remove(otherEnrolled.begin(), otherEnrolled.end(), outputAddr), otherEnrolled.end());
          WriteAddrCandidates(inputAddr, otherEnrolled);

          /// If there is nobody else, just set their votes to 0
          if (otherEnrolled.size() == 0) {

            Write(std::make_pair(DB_VOTE_COUNT, outputAddr), 0);

          } else {

            /// For each other enrolled address, update other candidates' vote count
            for (std::vector<std::string>::const_iterator ii = otherEnrolled.begin(); ii != otherEnrolled.end(); ++ii) {

              std::string otherCandidate = *ii;

              int otherCandidateVotes = 0;

              /// If they don't exist then add them and update their votes as if they had been voted for by this user
              if (!ReadVoteCount(otherCandidate, otherCandidateVotes))
                Write(std::make_pair(DB_VOTE_COUNT, otherCandidate), (uint64_t) (nAmount / (otherEnrolled.size() + 1)));

              /// Now update their vote count accordingly
              Write(std::make_pair(DB_VOTE_COUNT, otherCandidate),
                    (uint64_t) (otherCandidateVotes - nAmount / (otherEnrolled.size() + 1) +
                                nAmount / (otherEnrolled.size())));

            }

          }

        } else {
          /// Vote condition (this address is voting and not un-voting)

          otherEnrolled.insert(otherEnrolled.end(), outputAddr);
          WriteAddrCandidates(inputAddr, otherEnrolled);

          /// If there is nobody else, just set their votes to the voters' balance
          {

            /// For each other enrolled address, update other candidates' vote count
            for (std::vector<std::string>::const_iterator ii = otherEnrolled.begin(); ii != otherEnrolled.end(); ++ii) {

              std::string otherCandidate = *ii;

              int otherCandidateVotes = 0;

              /// If they don't exist then add them and update their votes as if they hadn't been voted for by this user
              if (!ReadVoteCount(otherCandidate, otherCandidateVotes))
                Write(std::make_pair(DB_VOTE_COUNT, otherCandidate), (uint64_t) (nAmount / otherEnrolled.size()));

              /// Now update their vote count accordingly
              if (otherCandidate != outputAddr) {
                Write(std::make_pair(DB_VOTE_COUNT, otherCandidate),
                      (uint64_t) (otherCandidateVotes - nAmount / (otherEnrolled.size()) +
                                  nAmount / (otherEnrolled.size() + 1)));

              } else {
                Write(std::make_pair(DB_VOTE_COUNT, otherCandidate),
                      (uint64_t) (otherCandidateVotes + nAmount / otherEnrolled.size()));
              }

            }

          }

        }

      }

    } else if (currTransaction->type == CTransactionTypes::VALUE && !(**it).IsCoinBase()) {

      UniValue entry(UniValue::VOBJ);
      TxToUniv(**it, (block->GetBlockHeader()).GetHash(), entry);

      /**
      * Subtract from inputAddr's candidates the value of the transaction / the amount of other enrolled addresses
      * Add to outputAddr's candidates the value of transaction / the amount of other enrolled addresses
      * Update each balance accordingly
      */

      std::string inputAddr = entry["vin"][0]["scriptSig"]["asm"].get_str();
      std::cout << "Received input address for value type: " << inputAddr << std::endl;
      std::vector<std::string> inputCandidates;

      std::vector<std::string> outputAddrs;
      std::vector<float> outputBalances;

      std::vector<UniValue> voutAddresses;
      std::string voutAssetType;

      /// Find the vout value, get all vouts
      /// std::vector<UniValue> voutObjects = find_value(entry, "vout").getValues();
      std::vector<UniValue> voutObjects = entry["vout"].getValues();
      std::cout << "Received vout objects, size: " << voutObjects.size() << std::endl;

      /// Support for one to many, TODO: need to check that this is the right way to get the addresses meant for vouts

      for (unsigned int j = 0; j < voutObjects.size(); j++) {

        voutAddresses = voutObjects[j]["scriptPubKey"]["addresses"].getValues();
        voutAssetType = voutObjects[j]["assetType"].get_str();

        for (unsigned int i = 0; i < voutAddresses[i].size(); i++) {
          if (voutAddresses[i].get_str().compare(inputAddr) != 0 && voutAssetType.compare(NATIVE_ASSET.c_str()) == 0) {
            outputBalances.insert(outputBalances.end(), strtof(voutObjects[j]["value"].get_str().c_str(), 0));
            std::cout << "Inserting output balance: " << voutObjects[j]["value"].get_str() << std::endl;
            outputAddrs.insert(outputAddrs.end(), voutAddresses[i].get_str());
          }
        }
      }

      float totalOutput = 0;
      totalOutput = std::accumulate(outputBalances.begin(), outputBalances.end(), 0.0f);
      /// First subtract from inputCandidates' candidate's vote balances
      /// If you can read the input address and find candidates (AKA they have voted for nodes)
      if(ReadAddrCandidates(inputAddr, inputCandidates)) {
        for(std::vector<std::string>::const_iterator ii = inputCandidates.begin(); ii != inputCandidates.end(); ++ii) {

          std::string candidateAddr = *ii;
          int nVotes = 0;

          /// If there is a vote count for this candidate then subtract from their balance the amount in the transaction'
          /// divided by the amount of candidates they are voting for (since the vote is split)
          if(ReadVoteCount(candidateAddr, nVotes)) {
            Write(std::make_pair(DB_VOTE_COUNT, candidateAddr), (uint64_t) (nVotes - totalOutput/inputCandidates.size()));
          }
          /// Otherwise, do nothing. Maybe the vote count for this address hasn't come in yet, who knows. If they have
          /// been voted for by this address and there is an output from this address (CTransactionTypes::VALUE) but
          /// they aren't in the database then they cannot be updated accurately.
        }
      }


      /// Then add to each outputCandidate's vote count
      /// At the end subtract / add from the two parties' balances in the database

      for(unsigned int i = 0; i < outputAddrs.size(); i++) {
        int nVotes = 0;

        /// Typical "If this doesn't have a value, this is the correct place to initialize the value in the database"
        ReadVoteCount(outputAddrs[i], nVotes);
        Write(std::make_pair(DB_VOTE_COUNT, outputAddrs[i]), (uint64_t) (nVotes + outputBalances[i]/inputCandidates.size()));

        // now add to their balances
        int nBalance = 0;
        ReadAddrBalance(outputAddrs[i], nBalance);
        Write(std::make_pair(DB_ADDR_BAL, outputAddrs[i]), (uint64_t) (nBalance + outputBalances[i]));
      }

      int nBalance = 0;
      ReadAddrBalance(inputAddr, nBalance);
      Write(std::make_pair(DB_ADDR_BAL, nBalance), (uint64_t) (nBalance - totalOutput));

    } else if (currTransaction->type == CTransactionTypes::FREEZE_ASSET) {

      UniValue entry(UniValue::VOBJ);
      TxToUniv(**it, (block->GetBlockHeader()).GetHash(), entry);

      std::string vinTxId = entry["vin"][0]["txid"].get_str();
      uint256 hash;
      hash.SetHex(vinTxId);
      CTransactionRef txOut;
      uint256 blockhash;

      g_txindex->FindTx(hash, blockhash, txOut);

      // DO NOT TRY TO FIND HASH OF TX CAUSES SEGFAULT
      bool frozen = false;

      std::string sAsset(currTransaction->attr.assetType.c_str());

      bool doesHaveOutputToCorrectAddress = false;
      std::string correctAssetType = "";
      std::vector<UniValue> outValues = entry["vout"].getValues();

      for(std::vector<UniValue>::const_iterator ii = outValues.begin(); ii != outValues.end(); ++ii) {
        if((*ii)["assetType"].get_str().compare((*ii)["scriptPubKey"]["addresses"][0].get_str()) == 0) {
          correctAssetType = (*ii)["assetType"].get_str();
          doesHaveOutputToCorrectAddress = true;
        }
      }



      if(ReadAssetFrozen(sAsset, frozen) && doesHaveOutputToCorrectAddress)
        WriteAssetFrozen(sAsset, !frozen);
      else WriteAssetFrozen(sAsset, frozen);

    }

  }
  return true;
}

bool CBlockTreeDB::ReadBlockFileInfo(int nFile, CBlockFileInfo &info) {
    return Read(std::make_pair(DB_BLOCK_FILES, nFile), info);
}

bool CBlockTreeDB::WriteReindexing(bool fReindexing) {
    if (fReindexing)
        return Write(DB_REINDEX_FLAG, '1');
    else
        return Erase(DB_REINDEX_FLAG);
}

bool CBlockTreeDB::ReadReindexing(bool &fReindexing) {
    fReindexing = Exists(DB_REINDEX_FLAG);
    return true;
}

bool CBlockTreeDB::ReadLastBlockFile(int &nFile) {
    return Read(DB_LAST_BLOCK, nFile);
}

CCoinsViewCursor *CCoinsViewDB::Cursor() const
{
    CCoinsViewDBCursor *i = new CCoinsViewDBCursor(const_cast<CDBWrapper&>(db).NewIterator(), GetBestBlock());
    /* It seems that there are no "const iterators" for LevelDB.  Since we
       only need read operations on it, use a const-cast to get around
       that restriction.  */
    i->pcursor->Seek(DB_COIN);
    // Cache key of first record
    if (i->pcursor->Valid()) {
        CoinEntry entry(&i->keyTmp.second);
        i->pcursor->GetKey(entry);
        i->keyTmp.first = entry.key;
    } else {
        i->keyTmp.first = 0; // Make sure Valid() and GetKey() return false
    }
    return i;
}

bool CCoinsViewDBCursor::GetKey(COutPoint &key) const
{
    // Return cached key
    if (keyTmp.first == DB_COIN) {
        key = keyTmp.second;
        return true;
    }
    return false;
}

bool CCoinsViewDBCursor::GetValue(Coin &coin) const
{
    return pcursor->GetValue(coin);
}

unsigned int CCoinsViewDBCursor::GetValueSize() const
{
    return pcursor->GetValueSize();
}

bool CCoinsViewDBCursor::Valid() const
{
    return keyTmp.first == DB_COIN;
}

void CCoinsViewDBCursor::Next()
{
    pcursor->Next();
    CoinEntry entry(&keyTmp.second);
    if (!pcursor->Valid() || !pcursor->GetKey(entry)) {
        keyTmp.first = 0; // Invalidate cached key after last record so that Valid() and GetKey() return false
    } else {
        keyTmp.first = entry.key;
    }
}

bool CBlockTreeDB::WriteBatchSync(const std::vector<std::pair<int, const CBlockFileInfo*> >& fileInfo, int nLastFile, const std::vector<const CBlockIndex*>& blockinfo) {
    CDBBatch batch(*this);
    for (std::vector<std::pair<int, const CBlockFileInfo*> >::const_iterator it=fileInfo.begin(); it != fileInfo.end(); it++) {
        batch.Write(std::make_pair(DB_BLOCK_FILES, it->first), *it->second);
    }
    batch.Write(DB_LAST_BLOCK, nLastFile);
    for (std::vector<const CBlockIndex*>::const_iterator it=blockinfo.begin(); it != blockinfo.end(); it++) {
        batch.Write(std::make_pair(DB_BLOCK_INDEX, (*it)->GetBlockHash()), CDiskBlockIndex(*it));
    }
    return WriteBatch(batch, true);
}

bool CBlockTreeDB::ReadTxIndex(const uint256 &txid, CDiskTxPos &pos) {
    return Read(std::make_pair(DB_TXINDEX, txid), pos);
}

bool CBlockTreeDB::WriteTxIndex(const std::vector<std::pair<uint256, CDiskTxPos> >&vect) {
    CDBBatch batch(*this);
    for (std::vector<std::pair<uint256,CDiskTxPos> >::const_iterator it=vect.begin(); it!=vect.end(); it++)
        batch.Write(std::make_pair(DB_TXINDEX, it->first), it->second);
    return WriteBatch(batch);
}

bool CBlockTreeDB::WriteFlag(const std::string &name, bool fValue) {
    return Write(std::make_pair(DB_FLAG, name), fValue ? '1' : '0');
}

bool CBlockTreeDB::ReadFlag(const std::string &name, bool &fValue) {
    char ch;
    if (!Read(std::make_pair(DB_FLAG, name), ch))
        return false;
    fValue = ch == '1';
    return true;
}

bool CBlockTreeDB::LoadBlockIndexGuts(const Consensus::Params& consensusParams, std::function<CBlockIndex*(const uint256&)> insertBlockIndex)
{
    std::unique_ptr<CDBIterator> pcursor(NewIterator());

    pcursor->Seek(std::make_pair(DB_BLOCK_INDEX, uint256()));

    // Load mapBlockIndex
    while (pcursor->Valid()) {
        boost::this_thread::interruption_point();
        std::pair<char, uint256> key;
        if (pcursor->GetKey(key) && key.first == DB_BLOCK_INDEX) {
            CDiskBlockIndex diskindex;
            if (pcursor->GetValue(diskindex)) {
                // Construct block index object
                CBlockIndex* pindexNew = insertBlockIndex(diskindex.GetBlockHash());
                pindexNew->pprev          = insertBlockIndex(diskindex.hashPrev);
                pindexNew->nHeight        = diskindex.nHeight;
                pindexNew->nFile          = diskindex.nFile;
                pindexNew->nDataPos       = diskindex.nDataPos;
                pindexNew->nUndoPos       = diskindex.nUndoPos;
                pindexNew->nVersion       = diskindex.nVersion;
                pindexNew->hashMerkleRoot = diskindex.hashMerkleRoot;
                pindexNew->nTime          = diskindex.nTime;
                pindexNew->nBits          = diskindex.nBits;
                pindexNew->nNonce         = diskindex.nNonce;
                pindexNew->nStatus        = diskindex.nStatus;
                pindexNew->nTx            = diskindex.nTx;

                if (!CheckProofOfWork(pindexNew->GetBlockHash(), pindexNew->nBits, consensusParams))
                    return error("%s: CheckProofOfWork failed: %s", __func__, pindexNew->ToString());

                pcursor->Next();
            } else {
                return error("%s: failed to read value", __func__);
            }
        } else {
            break;
        }
    }

    return true;
}

namespace {

//! Legacy class to deserialize pre-pertxout database entries without reindex.
class CCoins
{
public:
    //! whether transaction is a coinbase
    bool fCoinBase;

    //! unspent transaction outputs; spent outputs are .IsNull(); spent outputs at the end of the array are dropped
    std::vector<CTxOut> vout;

    //! at which height this transaction was included in the active block chain
    int nHeight;

    //! empty constructor
    CCoins() : fCoinBase(false), vout(0), nHeight(0) { }

    template<typename Stream>
    void Unserialize(Stream &s) {
        unsigned int nCode = 0;
        // version
        unsigned int nVersionDummy;
        ::Unserialize(s, VARINT(nVersionDummy));
        // header code
        ::Unserialize(s, VARINT(nCode));
        fCoinBase = nCode & 1;
        std::vector<bool> vAvail(2, false);
        vAvail[0] = (nCode & 2) != 0;
        vAvail[1] = (nCode & 4) != 0;
        unsigned int nMaskCode = (nCode / 8) + ((nCode & 6) != 0 ? 0 : 1);
        // spentness bitmask
        while (nMaskCode > 0) {
            unsigned char chAvail = 0;
            ::Unserialize(s, chAvail);
            for (unsigned int p = 0; p < 8; p++) {
                bool f = (chAvail & (1 << p)) != 0;
                vAvail.push_back(f);
            }
            if (chAvail != 0)
                nMaskCode--;
        }
        // txouts themself
        vout.assign(vAvail.size(), CTxOut());
        for (unsigned int i = 0; i < vAvail.size(); i++) {
            if (vAvail[i])
                ::Unserialize(s, CTxOutCompressor(vout[i]));
        }
        // coinbase height
        ::Unserialize(s, VARINT(nHeight, VarIntMode::NONNEGATIVE_SIGNED));
    }
};

}

/** Upgrade the database from older formats.
 *
 * Currently implemented: from the per-tx utxo model (0.8..0.14.x) to per-txout.
 */
bool CCoinsViewDB::Upgrade() {
    std::unique_ptr<CDBIterator> pcursor(db.NewIterator());
    pcursor->Seek(std::make_pair(DB_COINS, uint256()));
    if (!pcursor->Valid()) {
        return true;
    }

    int64_t count = 0;
    LogPrintf("Upgrading utxo-set database...\n");
    LogPrintf("[0%%]..."); /* Continued */
    uiInterface.ShowProgress(_("Upgrading UTXO database"), 0, true);
    size_t batch_size = 1 << 24;
    CDBBatch batch(db);
    int reportDone = 0;
    std::pair<unsigned char, uint256> key;
    std::pair<unsigned char, uint256> prev_key = {DB_COINS, uint256()};
    while (pcursor->Valid()) {
        boost::this_thread::interruption_point();
        if (ShutdownRequested()) {
            break;
        }
        if (pcursor->GetKey(key) && key.first == DB_COINS) {
            if (count++ % 256 == 0) {
                uint32_t high = 0x100 * *key.second.begin() + *(key.second.begin() + 1);
                int percentageDone = (int)(high * 100.0 / 65536.0 + 0.5);
                uiInterface.ShowProgress(_("Upgrading UTXO database"), percentageDone, true);
                if (reportDone < percentageDone/10) {
                    // report max. every 10% step
                    LogPrintf("[%d%%]...", percentageDone); /* Continued */
                    reportDone = percentageDone/10;
                }
            }
            CCoins old_coins;
            if (!pcursor->GetValue(old_coins)) {
                return error("%s: cannot parse CCoins record", __func__);
            }
            COutPoint outpoint(key.second, 0);
            for (size_t i = 0; i < old_coins.vout.size(); ++i) {
                if (!old_coins.vout[i].IsNull() && !old_coins.vout[i].scriptPubKey.IsUnspendable()) {
                    Coin newcoin(std::move(old_coins.vout[i]), old_coins.nHeight, old_coins.fCoinBase);
                    outpoint.n = i;
                    CoinEntry entry(&outpoint);
                    batch.Write(entry, newcoin);
                }
            }
            batch.Erase(key);
            if (batch.SizeEstimate() > batch_size) {
                db.WriteBatch(batch);
                batch.Clear();
                db.CompactRange(prev_key, key);
                prev_key = key;
            }
            pcursor->Next();
        } else {
            break;
        }
    }
    db.WriteBatch(batch);
    db.CompactRange({DB_COINS, uint256()}, key);
    uiInterface.ShowProgress("", 100, false);
    LogPrintf("[%s].\n", ShutdownRequested() ? "CANCELLED" : "DONE");
    return !ShutdownRequested();
}

TxIndexDB::TxIndexDB(size_t n_cache_size, bool f_memory, bool f_wipe) :
    CDBWrapper(GetDataDir() / "indexes" / "txindex", n_cache_size, f_memory, f_wipe)
{}

bool TxIndexDB::ReadTxPos(const uint256 &txid, CDiskTxPos& pos) const
{
    return Read(std::make_pair(DB_TXINDEX, txid), pos);
}

bool TxIndexDB::WriteTxs(const std::vector<std::pair<uint256, CDiskTxPos>>& v_pos)
{
    CDBBatch batch(*this);
    for (const auto& tuple : v_pos) {
        batch.Write(std::make_pair(DB_TXINDEX, tuple.first), tuple.second);
    }
    return WriteBatch(batch);
}

bool TxIndexDB::ReadBestBlock(CBlockLocator& locator) const
{
    bool success = Read(DB_BEST_BLOCK, locator);
    if (!success) {
        locator.SetNull();
    }
    return success;
}

bool TxIndexDB::WriteBestBlock(const CBlockLocator& locator)
{
    return Write(DB_BEST_BLOCK, locator);
}

/*
 * Safely persist a transfer of data from the old txindex database to the new one, and compact the
 * range of keys updated. This is used internally by MigrateData.
 */
static void WriteTxIndexMigrationBatches(TxIndexDB& newdb, CBlockTreeDB& olddb,
                                         CDBBatch& batch_newdb, CDBBatch& batch_olddb,
                                         const std::pair<unsigned char, uint256>& begin_key,
                                         const std::pair<unsigned char, uint256>& end_key)
{
    // Sync new DB changes to disk before deleting from old DB.
    newdb.WriteBatch(batch_newdb, /*fSync=*/ true);
    olddb.WriteBatch(batch_olddb);
    olddb.CompactRange(begin_key, end_key);

    batch_newdb.Clear();
    batch_olddb.Clear();
}

bool TxIndexDB::MigrateData(CBlockTreeDB& block_tree_db, const CBlockLocator& best_locator)
{
    // The prior implementation of txindex was always in sync with block index
    // and presence was indicated with a boolean DB flag. If the flag is set,
    // this means the txindex from a previous version is valid and in sync with
    // the chain tip. The first step of the migration is to unset the flag and
    // write the chain hash to a separate key, DB_TXINDEX_BLOCK. After that, the
    // index entries are copied over in batches to the new database. Finally,
    // DB_TXINDEX_BLOCK is erased from the old database and the block hash is
    // written to the new database.
    //
    // Unsetting the boolean flag ensures that if the node is downgraded to a
    // previous version, it will not see a corrupted, partially migrated index
    // -- it will see that the txindex is disabled. When the node is upgraded
    // again, the migration will pick up where it left off and sync to the block
    // with hash DB_TXINDEX_BLOCK.
    bool f_legacy_flag = false;
    block_tree_db.ReadFlag("txindex", f_legacy_flag);
    if (f_legacy_flag) {
        if (!block_tree_db.Write(DB_TXINDEX_BLOCK, best_locator)) {
            return error("%s: cannot write block indicator", __func__);
        }
        if (!block_tree_db.WriteFlag("txindex", false)) {
            return error("%s: cannot write block index db flag", __func__);
        }
    }

    CBlockLocator locator;
    if (!block_tree_db.Read(DB_TXINDEX_BLOCK, locator)) {
        return true;
    }

    int64_t count = 0;
    LogPrintf("Upgrading txindex database... [0%%]\n");
    uiInterface.ShowProgress(_("Upgrading txindex database"), 0, true);
    int report_done = 0;
    const size_t batch_size = 1 << 24; // 16 MiB

    CDBBatch batch_newdb(*this);
    CDBBatch batch_olddb(block_tree_db);

    std::pair<unsigned char, uint256> key;
    std::pair<unsigned char, uint256> begin_key{DB_TXINDEX, uint256()};
    std::pair<unsigned char, uint256> prev_key = begin_key;

    bool interrupted = false;
    std::unique_ptr<CDBIterator> cursor(block_tree_db.NewIterator());
    for (cursor->Seek(begin_key); cursor->Valid(); cursor->Next()) {
        boost::this_thread::interruption_point();
        if (ShutdownRequested()) {
            interrupted = true;
            break;
        }

        if (!cursor->GetKey(key)) {
            return error("%s: cannot get key from valid cursor", __func__);
        }
        if (key.first != DB_TXINDEX) {
            break;
        }

        // Log progress every 10%.
        if (++count % 256 == 0) {
            // Since txids are uniformly random and traversed in increasing order, the high 16 bits
            // of the hash can be used to estimate the current progress.
            const uint256& txid = key.second;
            uint32_t high_nibble =
                (static_cast<uint32_t>(*(txid.begin() + 0)) << 8) +
                (static_cast<uint32_t>(*(txid.begin() + 1)) << 0);
            int percentage_done = (int)(high_nibble * 100.0 / 65536.0 + 0.5);

            uiInterface.ShowProgress(_("Upgrading txindex database"), percentage_done, true);
            if (report_done < percentage_done/10) {
                LogPrintf("Upgrading txindex database... [%d%%]\n", percentage_done);
                report_done = percentage_done/10;
            }
        }

        CDiskTxPos value;
        if (!cursor->GetValue(value)) {
            return error("%s: cannot parse txindex record", __func__);
        }
        batch_newdb.Write(key, value);
        batch_olddb.Erase(key);

        if (batch_newdb.SizeEstimate() > batch_size || batch_olddb.SizeEstimate() > batch_size) {
            // NOTE: it's OK to delete the key pointed at by the current DB cursor while iterating
            // because LevelDB iterators are guaranteed to provide a consistent view of the
            // underlying data, like a lightweight snapshot.
            WriteTxIndexMigrationBatches(*this, block_tree_db,
                                         batch_newdb, batch_olddb,
                                         prev_key, key);
            prev_key = key;
        }
    }

    // If these final DB batches complete the migration, write the best block
    // hash marker to the new database and delete from the old one. This signals
    // that the former is fully caught up to that point in the blockchain and
    // that all txindex entries have been removed from the latter.
    if (!interrupted) {
        batch_olddb.Erase(DB_TXINDEX_BLOCK);
        batch_newdb.Write(DB_BEST_BLOCK, locator);
    }

    WriteTxIndexMigrationBatches(*this, block_tree_db,
                                 batch_newdb, batch_olddb,
                                 begin_key, key);

    if (interrupted) {
        LogPrintf("[CANCELLED].\n");
        return false;
    }

    uiInterface.ShowProgress("", 100, false);

    LogPrintf("[DONE].\n");
    return true;
}
