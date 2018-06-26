// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <primitives/transaction.h>

#include <hash.h>
#include <tinyformat.h>
#include <utilstrencodings.h>
#include <streams.h>
std::string COutPoint::ToString() const
{
    return strprintf("COutPoint(%s, %u)", hash.ToString().substr(0,10), n);
}

CTxIn::CTxIn(COutPoint prevoutIn, CScript scriptSigIn, uint32_t nSequenceIn)
{
    prevout = prevoutIn;
    scriptSig = scriptSigIn;
    nSequence = nSequenceIn;
}

CTxIn::CTxIn(uint256 hashPrevTx, uint32_t nOut, CScript scriptSigIn, uint32_t nSequenceIn)
{
    prevout = COutPoint(hashPrevTx, nOut);
    scriptSig = scriptSigIn;
    nSequence = nSequenceIn;
}

std::string CTxIn::ToString() const
{
    std::string str;
    str += "CTxIn(";
    str += prevout.ToString();
    if (prevout.IsNull())
        str += strprintf(", coinbase %s", HexStr(scriptSig));
    else
        str += strprintf(", scriptSig=%s", HexStr(scriptSig).substr(0, 24));
    if (nSequence != SEQUENCE_FINAL)
        str += strprintf(", nSequence=%u", nSequence);
    str += ")";
    return str;
}

CTxOut::CTxOut(const CAmount& nValueIn, CScript scriptPubKeyIn, CAssetType assetTypeIn)
{
    nValue = nValueIn;
    scriptPubKey = scriptPubKeyIn;
    assetType = assetTypeIn;
}

std::string CTxOut::ToString() const
{
    return strprintf("CTxOut(nValue=%d.%08d, assetType=%u, scriptPubKey=%s)", nValue / COIN, nValue % COIN, assetType, HexStr(scriptPubKey).substr(0, 30));
}

CTransactionAttributes::CTransactionAttributes(): type(CTransactionTypes::VALUE){}
CTransactionAttributes::CTransactionAttributes(CTransactionType txType): type(txType){}
CTransactionAttributes::CTransactionAttributes(CTransactionType txType, CAssetType assetType, CAmount assetTotalSupply, std::string assetSymbol): type(txType), assetType(assetType), assetTotalSupply(assetTotalSupply), assetSymbol(assetSymbol){}
    
CMutableTransaction::CMutableTransaction() : nVersion(CTransaction::CURRENT_VERSION), type(CTransactionTypes::VALUE), attr(CTransactionAttributes()), nLockTime(0) {}
CMutableTransaction::CMutableTransaction(const CTransaction& tx) : vin(tx.vin), vout(tx.vout), nVersion(tx.nVersion), type(tx.type), attr(tx.attr), nLockTime(tx.nLockTime) {}

uint256 CMutableTransaction::GetHash() const
{
    return SerializeHash(*this, SER_GETHASH, SERIALIZE_TRANSACTION_NO_WITNESS);
}

uint256 CTransaction::ComputeHash() const
{
    return SerializeHash(*this, SER_GETHASH, SERIALIZE_TRANSACTION_NO_WITNESS);
}

uint256 CTransaction::GetWitnessHash() const
{
    if (!HasWitness()) {
        return GetHash();
    }
    return SerializeHash(*this, SER_GETHASH, 0);
}

/* For backward compatibility, the hash is initialized to 0. TODO: remove the need for this default constructor entirely. */
CTransaction::CTransaction() : vin(), vout(), nVersion(CTransaction::CURRENT_VERSION), type(CTransactionTypes::VALUE), attr(CTransactionAttributes()), nLockTime(0), hash() {}
CTransaction::CTransaction(const CMutableTransaction &tx) : vin(tx.vin), vout(tx.vout), nVersion(tx.nVersion), type(tx.type), attr(tx.attr), nLockTime(tx.nLockTime), hash(ComputeHash()) {}
CTransaction::CTransaction(CMutableTransaction &&tx) : vin(std::move(tx.vin)), vout(std::move(tx.vout)), nVersion(tx.nVersion), type(tx.type), attr(std::move(tx.attr)), nLockTime(tx.nLockTime), hash(ComputeHash()) {}


CAmount CTransaction::GetValueOut(CAssetType assetType) const
{
    CAmount nValueOut = 0;
    for (const auto& tx_out : vout) {
        if (tx_out.assetType == assetType){
            nValueOut += tx_out.nValue;
            if (!MoneyRange(tx_out.nValue) || !MoneyRange(nValueOut))
                throw std::runtime_error(std::string(__func__) + ": value out of range");
        }
    }
    return nValueOut;
}

unsigned int CTransaction::GetTotalSize() const
{
    return ::GetSerializeSize(*this, SER_NETWORK, PROTOCOL_VERSION);
}

std::string CTransaction::ToString() const
{
    std::string str;
    str += strprintf("CTransaction(hash=%s, ver=%d, type=%u, vin.size=%u, vout.size=%u, nLockTime=%u)\n",
        GetHash().ToString().substr(0,10),
        nVersion,
        type,
        vin.size(),
        vout.size(),
        nLockTime);
    for (const auto& tx_in : vin)
        str += "    " + tx_in.ToString() + "\n";
    for (const auto& tx_in : vin)
        str += "    " + tx_in.scriptWitness.ToString() + "\n";
    for (const auto& tx_out : vout)
        str += "    " + tx_out.ToString() + "\n";
    return str;
}

std::string CTransactionAttributes::ToString() const
{
    std::string str;
    str += strprintf("CTransactionAttributes(type=%u)",type);
    
    return str;
}
