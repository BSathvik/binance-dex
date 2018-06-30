Binance Dex
=====================================


Binance Decentralized Exchange
----------------
We are submitting this project as a part of the [Binance DEXathon](https://medium.com/binanceexchange/binance-dexathon-845dc0cbfffe), 
a coding competition held by Binance with the goal of building a Decentralized exchange. Our implementation of a 
decentralized exchange is built off of Bitcoin, with many major changes to certain inner workings of bitcoin that 
improve its scalability. 

We removed Proof-of-Work and replaced it with a new implementation of Delegated Proof-of-Stake, a consensus algorithm 
that involves voting in block producers, who are selected to produce blocks on a schedule, in order to maintain a 
consistent blocktime while being able to handle many more transactions per second than most other consensus mechanisms. 
We will explain the tradeoffs of using this type of consensus in the "Design Rationale" section below. 

After implementing Delegated Proof-of-Stake, the main challenge of this project, we implemented the required market 
functions easily by creating new fields in transactions as well as new transaction types. In our case, market order 
transactions, asset transactions, asset creation, and native coin transactions are each different transaction types. We 
also added transaction attributes, allowing us to easily add the required information for each new transaction type. 
These attributes, as well as the way that we have separated the transaction types, make it very easy to add market 
features that are related to any type of value exchange, without using or needing any Ethereum or NEO-style smart 
contracts. Everything is baked directly into the blockchain.

For this reason, we can run an off-chain matching engine alongside a full node that is able to execute an atomic trade 
event. 

Design Rationale
----------------
For this project we made multiple design decisions in order to improve the performance of the exchange. Our main goal 
was to have as much throughput as possible, since liquidity is a necessity in any type of exchange.

There are pretty much two factors that effect throughput and exchange performance for decentralized exchanges specifically:
 - Consensus algorithm chosen (BIG factor, difficult to get right)
 - Off Chain vs On Chain matching engine
 
When we approached this massive project we knew that for people to use a decentralized exchange, it had to be extremely 
fast. We researched various consensus algorithms to try to find the fastest one. Here is a summary of our findings:

#### Proof-of-Work (PoW)
Had we chosen this consensus mechanism, we would have created the slowest decentralized exchange in existence. This is
the main drawback of Proof-of-Work (bitcoin's consensus mechanism), low throughput in exchange for high security and
potentially high decentralization. This was not optimal for a decentralized exchange. While it was already implemented 
in the bitcoin repo, we choose to disable that and implement a different consensus algorithm.

#### Proof-of-Stake (PoS)
Proof-of-Stake is, in theory, much faster than Proof-of-Work and is in general very fast. However, there are currently
no implementations of Proof-of-Stake that correctly address the Nothing at Stake Problem well. [Casper FFG](https://arxiv.org/abs/1710.09437), 
the consensus algorithm proposed by Vitalik Buterin and Virgil Griffith, addresses many of the problems with naive Proof-of-Stake
, including the Nothing at Stake problem but is very complicated and there are currently no working implementations. We
decided that if the Casper Authors could not implement Casper on the platform they invented and wrote in almost a year,
there was no way we could implement it in the Bitcoin repo in a couple months.

#### Delegated Proof-of-Stake (DPoS)
Delegated Proof-of-Stake is the consensus algorithm of EOS, Bitshares, and Steem, and uses a weighted voting-based consensus
algorithm. Nodes vote on "witnesses", such that the number of votes they are able to give is equal to their balance. The 
N witnesses with the most votes are then scheduled into time slots to produce blocks. This typeof consensus is very fast 
(thousands of transactions per second) but it does have some essential drawbacks, most notably:
 - It is "distributed," not completely decentralized in nature
 - Fails in a similar way to Proof-of-Stake, where block producers can collude with large stakeholders as well as other block
 producers, meaning there is no penalty for being malicious and creating an oligopolistic network. This is not addressed
 by any DPoS implementation currently.

We then realized that there is no formal definition or specification for DPoS. There are currently two implementations
of the protocol. One implementation is Lisk, the other is Dan Larimer's implementation (BitShares/Steem/EOS), which has
not changed for a very long time. While this is inconvenient, DPoS boasts large throughput with *relatively* low complexity,
so we chose to implement this consensus mechanism. This proved to be a challenge as there is currently no DPoS implementation
that uses an UTXO-based accounting model.

We considered using a graph-based structure with DPoS, such as Nano, but realized there is no clear incentive to run a node 
and therefore no transaction fees.

#### Delegated Byzantine Fault Tolerance (dBFT)
Delegated Byzantine Fault Tolerance works in a way similar to DPoS, with a few key differences. dBFT is currently used only
by the NEO platform, which we had some experience with at the start of this project. dBFT works by allowing nodes to enroll
as Delegates for a fixed fee. Nodes then vote (with stake, like DPoS) on these delegates to be a "bookkeeper" node. Out of these bookkeeper, a node 
is randomly chosen to be a speaker, or to build the block that 66% of the other bookkeepers reach consensus on. This is 
also extremely fast, and NEO currently operates with 0 transaction fees. However, when we looked through the NEO source code 
we found a problem:
 - We found no way to send vote transactions in the source code.
 - There are default, hard-coded bookkeeping nodes in NEO, and >51% of the stake is owned by the NEO council, meaning that
 the network is "owned" by the NEO council.
 
These issues led to us not using NEO or dBFT, as we couldn't rely on the consensus algorithm being secure on a large network
with public voting.

#### Conclusion on consensus:
We chose DPoS because it is simple, fast, and we had found solutions to the major consensus and governance issues that current
implementations have.

###Order Matching

Order matching is rarely a bottleneck for decentralized exchanges, since the consensus algorithm for distributed ledgers
is what leads to low transaction throughput and high latency.

Technical Details
----------------

Improvements, challenges, what we learned, and what we would do
----------------



Running a node using Docker
----------------
The Dockerfile builds an image with all the dependencies installed on Ubuntu 16.04 LTS.
Note: The Image doesn't have the binance-dex repo, you'll have to git clone it. Also, don't use `sudo` in the container.

1. Install Docker
2. Build the image by running `docker build -t binance-dex:first .`
3. To run image. `docker run -it -v <volume_name>:/home/ binance-dex:first`
4. Now, `git clone "https://github.com/BSathvik/binance-dex"`
5. Compile the repo


Bitcoin Core integration/staging tree
=====================================

[![Build Status](https://travis-ci.org/bitcoin/bitcoin.svg?branch=master)](https://travis-ci.org/bitcoin/bitcoin)

https://bitcoincore.org

What is Bitcoin?
----------------

Bitcoin is an experimental digital currency that enables instant payments to
anyone, anywhere in the world. Bitcoin uses peer-to-peer technology to operate
with no central authority: managing transactions and issuing money are carried
out collectively by the network. Bitcoin Core is the name of open source
software which enables the use of this currency.

For more information, as well as an immediately useable, binary version of
the Bitcoin Core software, see https://bitcoin.org/en/download, or read the
[original whitepaper](https://bitcoincore.org/bitcoin.pdf).

License
-------

Bitcoin Core is released under the terms of the MIT license. See [COPYING](COPYING) for more
information or see https://opensource.org/licenses/MIT.

Development Process
-------------------

The `master` branch is regularly built and tested, but is not guaranteed to be
completely stable. [Tags](https://github.com/bitcoin/bitcoin/tags) are created
regularly to indicate new official, stable release versions of Bitcoin Core.

The contribution workflow is described in [CONTRIBUTING.md](CONTRIBUTING.md).

Testing
-------

Testing and code review is the bottleneck for development; we get more pull
requests than we can review and test on short notice. Please be patient and help out by testing
other people's pull requests, and remember this is a security-critical project where any mistake might cost people
lots of money.

### Automated Testing

Developers are strongly encouraged to write [unit tests](src/test/README.md) for new code, and to
submit new unit tests for old code. Unit tests can be compiled and run
(assuming they weren't disabled in configure) with: `make check`. Further details on running
and extending unit tests can be found in [/src/test/README.md](/src/test/README.md).

There are also [regression and integration tests](/test), written
in Python, that are run automatically on the build server.
These tests can be run (if the [test dependencies](/test) are installed) with: `test/functional/test_runner.py`

The Travis CI system makes sure that every pull request is built for Windows, Linux, and OS X, and that unit/sanity tests are run automatically.

### Manual Quality Assurance (QA) Testing

Changes should be tested by somebody other than the developer who wrote the
code. This is especially important for large or high-risk changes. It is useful
to add a test plan to the pull request description if testing the changes is
not straightforward.

Translations
------------

Changes to translations as well as new translations can be submitted to
[Bitcoin Core's Transifex page](https://www.transifex.com/projects/p/bitcoin/).

Translations are periodically pulled from Transifex and merged into the git repository. See the
[translation process](doc/translation_process.md) for details on how this works.

**Important**: We do not accept translation changes as GitHub pull requests because the next
pull from Transifex would automatically overwrite them again.

Translators should also subscribe to the [mailing list](https://groups.google.com/forum/#!forum/bitcoin-translators).
