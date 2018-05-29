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
    return (tx.type == CTransaction::TYPE_VOTE);
}

bool inCurrentMaintenanceInterval(CBlockIndex *blockIndex, CChain &chainActive){
    
    //Check for block timestamps on top of block height.
    //int64_t nAdjustedTime = GetAdjustedTime();
    int64_t lastMaintenanceTime = chainActive.Tip()->GetBlockTime() - chainActive.Tip()->GetBlockTime() % MAINTENANCE_INTERVAL_BLOCK_TIME;

    int64_t lastMaintenanceHeight = chainActive.Height() - chainActive.Height() % MAINTENANCE_INTERVAL_BLOCK_HEIGHT;
    
    if (blockIndex->nHeight >= lastMaintenanceHeight && blockIndex->GetBlockTimeMax() >= lastMaintenanceTime)
        return true;
    
    return false;
}

CAmount GetTransactionVoteAmount(const CTransaction &voteTx, CBlockIndex *blockIndex, CChain &chainActive, const CWallet *pwallet) {

    if (!(blockIndex && chainActive.Contains(blockIndex) && chainActive.Height() - blockIndex->nHeight >= 0))
        return error("%s: Invalid CBlockIndex", __func__);
    
    if (!(isVoteTransaction(voteTx) && inCurrentMaintenanceInterval(blockIndex, chainActive)))
        return 0 * COIN;

    CAmount totalVoteCount = 0;
    for (const auto& txout : voteTx.vout)
    {
        //Check if the vout is meant for this node/wallet.
        if(pwallet->IsMine(txout))
            totalVoteCount += txout.nValue;
    }
    return totalVoteCount;
}
