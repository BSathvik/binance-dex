#include <countvotes.h>
#include <consensus/tx_verify.h>
#include <amount.h>
#include <coins.h>
#include <timedata.h>


static const int64_t MAINTENANCE_INTERVAL_BLOCK_TIME = 24 * 60 * 60;
static const uint64_t MAINTENANCE_INTERVAL_BLOCK_HEIGHT = 12 * 60 * 60; // Assuming a block is produced every 2 seconds.


bool isVoteTransaction(const CTransaction &tx){
    if (tx.type == CTransaction::TYPE_VOTE)
        return true;
    return false;
}

//You need context of the main chain to check if it in the maintenance interval, 
//the main active chain is in validation.cpp, lets just say we have it for now.
bool inMaintenanceInterval(CBlockHeader &block){
    int64_t nAdjustedTime = GetAdjustedTime();
    //TODO: Actually implement this
    return true;
    //if (block.GetBlockTime() >  + MAX_FUTURE_BLOCK_TIME)
}

// i guess I need to make sure that votetx is a voting transaction and that its nLockTime
// is before the maintenance interval stuff
CAmount GetTransactionVoteAmount(const CTransaction &voteTx, CBlockHeader &block) {
    
    if (!(isVoteTransaction(voteTx) && inMaintenanceInterval(block)))
        return 0 * COIN;
    
    //TODO: nLockTime checks. we need context to know when the next maintenance interval is.
    
    //TODO: Consensus::CheckTxInputs. Need context to call this. consensus/tx_verify.cpp
    
    CAmount totalVoteCount = 0;
        
    for (const auto& txout : voteTx.vout)
    {
        //TODO: Check if the vout is meant for this node/wallet.
        totalVoteCount += txout.nValue;
    }
    return totalVoteCount;
}
