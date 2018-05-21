#ifndef COUNTVOTES_H
#define COUNTVOTES_H

#include <primitives/transaction.h>
#include <wallet/wallet.h>
#include <chain.h>
#include <amount.h>
#include <coins.h>

CAmount GetTransactionVoteAmount(const CTransaction &voteTx, CBlockIndex *blockIndex, CChain &chainActive);

#endif // COUNTVOTES_H
