#ifndef COUNTVOTES_H
#define COUNTVOTES_H

#include <primitives/transaction.h>
#include <wallet/wallet.h>
#include <chain.h>

CAmount countedVotes(const CTransaction &voteTx, CBlockHeader &pblock);

#endif // COUNTVOTES_H
