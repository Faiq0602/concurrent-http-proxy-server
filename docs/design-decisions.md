# Design Decisions

## 1. Plain HTTP Before HTTPS CONNECT

The project implements plain HTTP proxying first because it demonstrates core systems concerns clearly:

- socket lifecycle
- request parsing
- request rewriting
- response relay
- concurrency
- shared caching

HTTPS CONNECT is intentionally deferred because it adds tunnel semantics and certificate-adjacent discussion without helping the initial project scope.

## 2. Thread Pool Instead Of Thread-Per-Connection

A bounded worker pool was chosen because it is a stronger systems design signal than unbounded thread creation.

Benefits:

- predictable concurrency
- easier resource reasoning
- explicit queueing behavior
- clearer scaling behavior under load

Tradeoff:

- queue management adds synchronization complexity

That tradeoff is acceptable because the code isolates the complexity inside `thread_pool.c`.

## 3. Shared Cache With Single Mutex

The cache uses one mutex around a hash table plus LRU list.

Benefits:

- simple correctness story
- easy to review
- low risk of subtle locking bugs

Tradeoff:

- less scalable than a sharded or lock-free design

For this project stage, clarity is more important than maximum throughput.

## 4. Fixed-Size Buffers In Parsing Path

The parser and request intake use bounded buffers with explicit limits.

Benefits:

- easier safety reasoning in C
- simpler malformed-request handling
- clearer limits and tradeoffs in the implementation

Tradeoff:

- very large request headers are rejected

This is acceptable and is surfaced explicitly through error handling.

## 5. Cache Only After Full Successful Relay

Responses are cached only after the miss-path relay completes successfully.

Benefits:

- avoids caching partial responses
- keeps cache contents aligned with what clients actually received
- simplifies ownership and failure handling

Tradeoff:

- no streaming population into cache during relay

That tradeoff is intentional for correctness and readability.

## 6. Metrics As Internal Counters, Not An Endpoint

Metrics are exposed through logs and shutdown summary rather than an HTTP stats endpoint.

Benefits:

- smaller scope
- no extra protocol surface
- easy local verification

Tradeoff:

- operational data is less queryable than a dedicated endpoint

For this repository stage, log-based observability is sufficient.

## 7. Documentation Follows Implementation

The docs are added after the core system is already functioning.

Benefits:

- architecture notes reflect actual code rather than plans
- easier for reviewers to map design statements to implementation

Tradeoff:

- early PRs had less repository-level explanation

That tradeoff fits the PR-by-PR workflow and keeps each step honest.

## Current Limitations

- no HTTPS CONNECT support
- no persistent connections
- no chunked-transfer-specific handling beyond raw relay
- no metrics export endpoint
- cache eligibility is intentionally simple

## Planned Next Areas

- broader protocol support if project scope expands
- richer cache eligibility rules if operational complexity becomes worthwhile
- deeper observability only if it justifies the added surface area
