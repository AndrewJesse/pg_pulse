# pg_pulse

pg_pulse is a Postgres health monitor and shard-routing prototype built around PostgreSQL 18 extensions, a pgrx background worker, and Kubernetes deployment patterns.

## Quick start

- Local: `make up`
- Kind: `make kind-up && make argocd-up`
- AWS: `make aws-bootstrap && make aws-up`

## Repository layout

See the project spec in [AGENTS.md](AGENTS.md).

## Local development

1. Install PostgreSQL 18 tooling, Rust, Docker, and kind.
2. Run `make build`.
3. Run `make test` and `make lint`.

## AWS notes

The infrastructure is intentionally small and pausable. See the design rules in [AGENTS.md](AGENTS.md) and the cost notes in [docs/aws-costs.md](docs/aws-costs.md).
