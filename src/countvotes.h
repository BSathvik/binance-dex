#ifndef COUNTVOTES_H
#define COUNTVOTES_H

#include <primitives/transaction.h>
#include <wallet/wallet.h>
#include <chain.h>
#include <amount.h>
#include <coins.h>

CAmount GetTransactionVoteAmount(const CTransaction &voteTx, CBlockIndex *blockIndex, CChain &chainActive, const CWallet *pwallet);

#endif // COUNTVOTES_H
