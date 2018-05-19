#include <countvotes.h>

// i guess I need to make sure that votetx is a voting transaction and that its nLockTime
// is before the maintenance interval stuff
CTransaction countedVotes(const CTransaction voteTx, CBlockHeader* pblock) {

  int64_t minBlockTime = pblock->GetBlockTime(); // minus maintenance interval
  // getting input transactions
  std::vector<CTxOut> outputs = voteTx.vout;
  for (std::vector<CTxOut>::iterator it = inputs.begin(); it != inputs.end; ++it) {
	// it may be clear that I don't know what I'm doing
	if(outputs[it].IsNull()) inputs.erase(inputs.begin() + it);
    //maybe i need to work more with CTransactions, etc
  }


}
