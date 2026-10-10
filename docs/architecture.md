# Architecture

This project is structured around a PostgreSQL 18 primary database, logical decoder, health worker, router, and optional rules engine.

## Components

- pulse_core: executor-level query tracking
- pulse_decoder: logical decoding JSON output
- pulse_health: background checking and alerting
- router: shard routing and transaction pinning
- rules-engine: sandboxed TypeScript evaluations
