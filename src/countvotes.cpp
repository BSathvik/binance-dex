#include <countvotes.h>
#include <consensus/tx_verify.h>
#include <consensus/consensus.h>
#include <amount.h>
#include <coins.h>
#include <timedata.h>
#include <validation.h>
#include <util.h>


static const int64_t MAINTENANCE_INTERVAL_BLOCK_TIME = 24 * 60 * 60;
static const int64_t MAINTENANCE_INTERVAL_BLOCK_HEIGHT = 12 * 60 * 60; // Assuming a block is produced every 2 seconds.


bool isVoteTransaction(const CTransaction &tx){
    if (tx.type == CTransaction::TYPE_VOTE)
        return true;
    return false;
}

bool inCurrentMaintenanceInterval(CBlockIndex *blockIndex, CChain &chainActive){
    
    //TODO: Check for block timestamps on top of block height.
    //int64_t nAdjustedTime = GetAdjustedTime();
    
    int64_t lastMaintenanceHeight = chainActive.Height() - chainActive.Height() % MAINTENANCE_INTERVAL_BLOCK_HEIGHT;
    
    if (blockIndex->nHeight >= lastMaintenanceHeight)
        return true;
    
    return false;
}

CAmount GetTransactionVoteAmount(const CTransaction &voteTx, CBlockIndex *blockIndex, CChain &chainActive) {

    if (!(blockIndex && chainActive.Contains(blockIndex) && chainActive.Height() - blockIndex->nHeight >= 0))
        return error("%s: Invalid CBlockIndex", __func__);
    
    if (!(isVoteTransaction(voteTx) && inCurrentMaintenanceInterval(blockIndex, chainActive)))
        return 0 * COIN;

    //TODO: Consensus::CheckTxInputs. Need context to call this. consensus/tx_verify.cpp
    CAmount totalVoteCount = 0;
    for (const auto& txout : voteTx.vout)
    {
        //TODO: Check if the vout is meant for this node/wallet.
        totalVoteCount += txout.nValue;
    }
    return totalVoteCount;
}
