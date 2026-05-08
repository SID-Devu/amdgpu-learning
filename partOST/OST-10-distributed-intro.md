# OST 10 — Distributed systems intro: consensus, RPC, time

> "A distributed system is one in which the failure of a computer you didn't even know existed can render your own computer unusable." — Leslie Lamport. A short tour for the kernel-bound engineer.

## Why a kernel engineer should care

GPUs run in clusters. Multi-GPU systems share work. Drivers talk to firmware over rings. Firmware runs its own scheduler. Even within a single machine, the **GPU and CPU are a distributed system** (no shared coherent memory in many cases; messages, fences, queues).

Concepts here will recur:
- consensus → who's writing to a shared queue?
- time → ordering events without a global clock.
- failure modes → silent corruption vs explicit error.
- caching → invalidation and consistency.

## Definitions

A **distributed system** is multiple independent computers communicating over a network, usually:
- without shared memory,
- with possible network failures (lost, delayed, reordered messages),
- with possible node failures (crash-stop, crash-recover, byzantine),
- with no global clock.

These properties make distributed systems **harder than threads**.

## The fundamental impossibility — FLP

Fischer, Lynch, Paterson (1985): in an asynchronous system with even one faulty process, no algorithm can guarantee both **safety** and **liveness** for consensus.

Practical consequence: every consensus protocol either:
- has timeouts (gives up liveness eventually),
- assumes synchrony,
- weakens guarantees.

## CAP theorem

Brewer: in a distributed system, you can pick at most **2 of**:
- **Consistency** (all reads see most recent write),
- **Availability** (every request gets a response),
- **Partition tolerance** (works when network splits).

Since networks **always** partition (briefly, eventually), the real choice is **C vs A** during a partition.

- CP: refuse responses to ensure consistency (e.g., consensus-based systems like etcd, Spanner).
- AP: respond with possibly-stale data (e.g., DynamoDB, Cassandra).

CAP is a frame; nuance lives in how a system handles partitions when they happen.

## Time and ordering

Without a global clock, "what happened first?" is hard.

### Lamport timestamps
Each message: sender's logical time + 1 → receiver's. Gives a partial order ("happened-before"). Doesn't detect concurrency.

### Vector clocks
Each node tracks a vector of seen events from every node. Detects concurrent events. Used in Dynamo, Riak.

### Hybrid logical clocks (HLC)
Combine wall clock + counter; tightly bounded skew + monotonic. Used in CockroachDB, MongoDB.

### Real clocks
GPS / atomic-clock synced clocks (Google Spanner's TrueTime). Provides bounded uncertainty windows. Linearizable transactions across continents.

## Consensus algorithms

The classic problem: a set of nodes must agree on a value.

### Two-phase commit (2PC)
Coordinator asks all participants "can you commit?" If all yes → commit. Else abort.

Vulnerability: coordinator dies after some commits but before all → blocked. Real DBs use 3PC or external resolution.

### Paxos
Lamport, 1998. Solves consensus despite up-to-f failures with 2f+1 nodes. Notoriously hard to understand in original paper. "Paxos Made Simple" is the canonical readable version.

### Raft
2014. Designed to be **understandable**. Leader-based; if the leader dies, an election picks a new one. Replicates a log via simple AppendEntries RPC.

Used in: etcd, Consul, CockroachDB, RethinkDB, TiKV.

### Byzantine fault tolerance (BFT)
For nodes that may **lie** (malicious / compromised). PBFT, Tendermint. Used in some blockchains. Heavier (3f+1 nodes for f Byzantine failures).

## RPC (Remote Procedure Call)

Make a function call appear remote. Pioneered by Birrell & Nelson (1984). Modern incarnations: gRPC, Cap'n Proto, Thrift.

Key issues:
- Failures: did the call succeed if you got no response? You don't know.
- Idempotency: can you safely retry? Design APIs so retry is safe.
- Latency: vastly higher than local calls; don't pretend they're free.
- Serialization: protobuf, JSON, msgpack.

## Replication

Multiple copies of data for availability + durability.

### Single-leader (primary/backup)
All writes go to leader; leader replicates to followers. Simple. Strong consistency on reads from leader.

### Multi-leader
Multiple leaders accept writes; sync to each other. Used in geo-replication. Conflict resolution is hard (CRDTs, last-writer-wins, manual merge).

### Leaderless
Writes go to multiple nodes; reads query multiple. Quorum-based. Dynamo, Cassandra.

## Quorums

W = write replicas, R = read replicas, N = total. For strong consistency: **W + R > N**. Common choice: W = R = N/2 + 1.

## Eventual consistency

"After a quiescent period, all replicas converge." Used in DynamoDB, Cassandra. Cheap, scalable, occasional inconsistency. Useful when application can tolerate it (shopping cart, social posts).

## Stronger models (reading)

- **Linearizability**: every operation appears to take effect at a single instant between its invocation and response. Like a single shared variable.
- **Sequential consistency**: all operations appear in some total order, consistent with each thread's order.
- **Causal consistency**: causally related operations are seen in order.
- **Eventual consistency**: weakest non-trivial.

In practice, **read your own writes** is what users actually expect.

## CRDTs (Conflict-free Replicated Data Types)

Data structures that always converge under arbitrary merge order. Examples: G-counter (grow-only counter), OR-set (observed-remove set), LWW-register.

Used in Riak, Redis enterprise, collaborative editors (Yjs, Automerge).

## Failure detectors

How do you know a remote node is dead?
- **Heartbeats**: periodic "I'm alive."
- **Phi-accrual**: probabilistic measure of likelihood of failure.

False positives (think dead, actually alive) → split-brain. False negatives (alive, really dead) → unavailability.

## Saga / transactions across services

Microservice "transactions" can't use 2PC across DBs. Pattern: **saga** — sequence of local transactions with compensating actions. Tolerate intermediate states.

## Real-world systems to know about

- **etcd / Consul** — Raft-based config stores. Used by Kubernetes for cluster state.
- **ZooKeeper** — older Zab-based. Used by Hadoop, Kafka.
- **Spanner** — Google's globally-consistent DB. TrueTime-based.
- **CockroachDB / TiKV / FoundationDB** — distributed SQL with serializability.
- **Cassandra / DynamoDB / ScyllaDB** — eventually-consistent KV.
- **Kafka** — distributed log (per-partition leader, replicated).

## What this means for GPUs

GPU drivers contain "small distributed systems":
- CPU and GPU as two nodes (with messaging via rings).
- Multi-GPU coherence (NCCL, RCCL).
- SR-IOV virtualization (host + N guest "drivers" sharing one physical GPU).
- Firmware on the GPU (PSP, SMU) running concurrently.

Concepts like fences, sequence numbers, timeouts directly mirror distributed systems patterns.

## Try it (mostly reading)

1. Read the Raft paper "In Search of an Understandable Consensus Algorithm." Watch the Raft visualization (https://raft.github.io/).
2. Read Brewer's "CAP twelve years later" article — short, by the originator.
3. Try `etcd` locally: start 3 nodes; partition one with `iptables`; observe behavior.
4. Read "There's just no getting around it: you're building a distributed system" — Mark Cavage.
5. Read *Designing Data-Intensive Applications* by Kleppmann at least chapters 5, 7, 9.

## Read deeper

- *Designing Data-Intensive Applications* — Kleppmann. Best modern survey.
- *Distributed Systems* by van Steen & Tanenbaum.
- Lamport's papers (free online): "Time, Clocks, and the Ordering of Events," "Paxos Made Simple."
- Raft paper: https://raft.github.io/raft.pdf

## Closing the OS theory part

You now have:
- **Process model** (formal).
- **Scheduling theory** (algorithms beyond CFS).
- **Synchronization theory** (Peterson, semaphores, monitors, modern atomics).
- **Classic problems** (producer/consumer, dining philosophers, etc.).
- **Deadlock** (prevention, detection, lockdep).
- **Memory** (paging, address spaces, NUMA).
- **Page replacement** (LRU and beyond).
- **Filesystems** (inodes, extents, journaling, CoW).
- **Crash consistency** (atomic rename, fsync rules).
- **Distributed systems** (consensus, time, CAP).

That's a real OS theory foundation.

The **next stage** of the [LEARNING PATH](../LEARNING-PATH.md) is **Stage 9 — Linux kernel foundations** ([Part V](../part5-kernel/)). You're ready.
