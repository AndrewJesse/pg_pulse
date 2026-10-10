# Runbook

## Local operations

- Start the stack with `make up`.
- Inspect health with `make e2e`.

## Recovery notes

- Logical decoding output depends on replica identity semantics.
- Restarting `shared_preload_libraries` requires a PostgreSQL restart.
