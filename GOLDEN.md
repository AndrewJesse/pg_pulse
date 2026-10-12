# GOLDEN

The canonical path for understanding this repo and for shipping a feature that touches every part of it.

## 1. Understand the repo (read in this order)

1. `README.md`: what pg_pulse is and the quick-start commands.
2. `AGENTS.md`: the build spec, versions, hard rules (AWS cost rules, never deploy the chaos build via Argo CD), and milestones.
3. `docs/architecture.md`, then `docs/gucs.md`, `docs/runbook.md`, `docs/aws-costs.md`, and `docs/decisions/` (ADRs explain deviations).
4. `Makefile`: every build, test, deploy, chaos, and AWS entry point.
5. `extensions/pulse_core` (C): executor hooks and shared-memory stats. Read `src/pulse_core.c`, `shmem.c`, `tracker.c`, `sqlapi.c`, the SQL in `pulse_core--0.1.0.sql`, and `test/`.
6. `extensions/pulse_decoder` (C): logical decoding output plugin.
7. `extensions/pulse_health` (Rust/pgrx): background worker for bloat, vacuum lag, xmin horizon, WAL growth.
8. `router/` (Rust/tokio): wire-protocol proxy, routing, pooling. See `src/{listener,session,routing,pool,protocol}` and `config.example.yaml`.
9. `rules-engine/` (Rust + deno_core): sandboxed TypeScript alert rules. See `src/{poller,sandbox}.rs` and `rules/`.
10. `docker/`: images for postgres, router, rules-engine.
11. `deploy/`: Kustomize `base/` (CNPG db, shards, router, rules-engine), `overlays/{ci,dev,prod}`, and `argocd/` (GitOps).
12. `infra/` and `scripts/`: AWS Terraform, bootstrap, kind/Argo CD scripts, e2e, power control.
13. `chaos/`: crash and recovery drills (kill -9, core dumps, pgbench load, recovery verification).
14. `.github/workflows/`: CI, e2e, chaos, infra, release, promote.

Start local: `make build && make test && make lint`, then `make up` or `make kind-up && make argocd-up`.

## 2. Add a feature end to end

Example feature: track a new metric (`temp_bytes`) and alert on it.

1. **Record the decision.** If it deviates from `AGENTS.md`, add an ADR in `docs/decisions/NNNN-title.md`.
2. **pulse_core (C).** Add the field in `tracker.h`/`tracker.c`, capture it in the hook in `pulse_core.c`, expose it in `sqlapi.c` and `pulse_core--0.1.0.sql`. Add a regression case under `test/sql` and `test/expected`. Run `make test-c`.
3. **pulse_decoder (C).** If the change is visible in row changes, update the JSON output and its tests.
4. **pulse_health (Rust/pgrx).** Add the check or signal in the background worker; add a GUC if tunable and document it in `docs/gucs.md`. Run `make test-health`.
5. **router.** If routing or pooling is affected, update `src/` and `config.example.yaml`, with tests in `router/tests`. Expose metrics in `metrics.rs`.
6. **rules-engine.** Surface the new data in `poller.rs`, and add or update an example rule in `rules/`. Run `make test-rust`.
7. **Images.** Update `docker/` if build deps changed; `make image`.
8. **Deploy.** Update `deploy/base` manifests/config, then each overlay (`ci`, `dev`, `prod`) as needed. Validate with `kustomize build`. Verify on kind: `make kind-up && make argocd-up` (or `make dev-deploy`).
9. **Chaos.** Add or extend a scenario in `chaos/` if the feature touches crash recovery; run `make chaos` against the chaos image only.
10. **Infra (only if needed).** Terraform in `infra/aws`; respect the AWS cost rules in `AGENTS.md` and `docs/aws-costs.md`.
11. **Docs.** Update `docs/architecture.md`, `docs/runbook.md`, and `docs/gucs.md`.
12. **CI and ship.** Run `make build test lint`, ensure workflows cover the new paths, commit with Conventional Commits (`feat(pulse_core): ...`), open a PR, wait for green.

Definition of done: C and Rust tests pass, kind deployment healthy, chaos recovery verified where relevant, docs and ADRs updated, CI green.
