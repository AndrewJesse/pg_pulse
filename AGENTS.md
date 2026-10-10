# pg_pulse: Agent Build Spec

> Put this file at the repo root as `AGENTS.md` (or `CLAUDE.md` for Claude Code).
> It is the single source of truth for building the project. Read all of it before writing code.

---

## 0. Instructions for the agent

1. Work **milestone by milestone** (section 10). Do not start a milestone until the previous one's acceptance criteria pass in CI.
2. One commit (or PR) per milestone step. Use Conventional Commits (`feat(pulse_core): ...`, `ci: ...`, `infra: ...`).
3. **Never guess version-specific Postgres APIs.** Before writing a hook, callback, or helper call, open the PostgreSQL 18 headers (`$(pg_config --includedir-server)`) and use these upstream files as reference implementations:

- `contrib/pg_stat_statements/` for executor hooks + shared memory + LWLocks
- `contrib/test_decoding/` for the logical decoding output plugin
- `src/test/modules/worker_spi/` for background workers in C (pgrx has its own API, see 5.3)
4. Pin every version (section 2). If something here is out of date when you run, pick the latest **patch** of the stated **major/minor**, and record the change in `docs/decisions/`.
5. Never commit secrets. Never put secrets in Terraform (they end up in state). Never deploy the chaos build (section 6.2) through Argo CD.
6. **AWS cost rules are hard rules.** Only create the AWS resources listed in section 8. No EKS, NAT gateways, load balancers, Elastic IPs, VPC interface endpoints, RDS, or ECR unless an ADR justifies it and the user approves.
7. If a requirement here is impossible or wrong, write an ADR in `docs/decisions/NNNN-title.md` explaining the deviation, then continue.
8. Keep a `PG_MAJOR=18` variable in the Makefile, Dockerfiles, and CI so moving to PostgreSQL 19 later is a one-line change.

---

## 1. What we are building

**pg_pulse** is a Postgres health monitor that lives inside the database, plus a small shard router. It exists to practice the low-level skills Supabase hires for: Postgres internals, C and Rust extensions, shared memory, background workers, WAL and logical decoding, crash recovery, debugging core dumps, wire protocol, and connection pooling.

It is deployed to Kubernetes with CloudNativePG, managed by Argo CD (GitOps), built/tested/released by GitHub Actions, and hosted on **one small, pausable AWS EC2 instance running k3s**. Locally and in CI it runs on kind.

Component
Language
Purpose

`pulse_core`
C
Executor hooks record per-query stats into a shared memory hash table

`pulse_decoder`
C
Logical decoding output plugin that emits row changes as JSON lines

`pulse_health`
Rust (pgrx)
Background worker that detects bloat, vacuum lag, xmin horizon, and WAL growth

`router`
Rust (tokio)
Postgres wire-protocol proxy: shard routing + transaction pooling (stretch)

`rules-engine`
Rust + deno_core
Runs user TypeScript alert rules in sandboxed V8 isolates (stretch)

---

## 2. Versions (as of October 2026, verify latest patch)

Tool
Version
Notes

PostgreSQL
**18.x** (18.6 current)
PG 19 GA is scheduled for late Oct 2026. Do not target 19 until GA **and** pgrx supports it.

pgrx / cargo-pgrx
**0.19.x**
`cargo-pgrx` version must exactly match the `pgrx` crate version. pgrx has no MSRV policy: use latest stable Rust.

Rust
latest stable
`rust-toolchain.toml` pins it.

CloudNativePG operator
**1.30.x**
Install via Helm chart, pinned.

Argo CD
**3.5.x**
Install via pinned upstream manifest + kustomize patches.

Kubernetes (local/CI)
kind, default node image

Kubernetes (AWS)
k3s, pinned release from the stable channel
Single node.

Terraform
**1.10+**
S3 backend with native locking (`use_lockfile = true`), no DynamoDB table.

AWS provider
`hashicorp/aws` 6.x (verify latest)
`default_tags` on everything.

AWS OS
Ubuntu 24.04 LTS **arm64** (Graviton)
AMI looked up from the Canonical SSM public parameter.

Base images
`postgres:18-trixie` (builder), `ghcr.io/cloudnative-pg/postgresql:18.<patch>-standard-trixie` (runtime)
Verify exact tags and that **arm64** is published. Builder and runtime must share PG major **and** Debian release.

Registry
GHCR
`ghcr.io/<owner>/pg-pulse-postgres`, `-router`, `-rules`. Multi-arch (amd64 + arm64). No ECR.

Add `renovate.json` so these pins get update PRs (including Terraform providers and k3s).

---

## 3. Architecture

```
        GitHub repo (main) <-------------------------------+
           |  push / PR                                     | CI bumps image tags
           v                                                | in deploy/overlays/dev
     GitHub Actions --- multi-arch images ------> GHCR      |
           |  kind e2e + chaos run here ($0 AWS)     |      |
           |                                         | pull |
           | OIDC: terraform apply, start/stop       |      |
           v                                         v      |
   AWS us-east-1 (default)                                  |
   +-----------------------------------------------------+  |
   | VPC: 1 public subnet, no NAT, SG with NO inbound    |  |
   |  EC2 t4g.large, Ubuntu arm64, k3s                   |  |
   |   argocd ns: Argo CD  -- polls git -----------------+--+
   |   cnpg-system ns: CloudNativePG operator  (wave -1) |
   |   pg-pulse-dev ns:                                  |
   |     Cluster pulse-main   <- pulse_core, pulse_health|
   |        logical slot -> pulse_decoder JSON stream    |
   |     Cluster pulse-shard-0, pulse-shard-1 (wave 0)   |
   |     router :6432 -> shards (wave 1), rules-engine   |
   |  EventBridge Scheduler: nightly auto-stop           |
   |  AWS Budget: email alerts                           |
   +-----------------------------------------------------+
        ^
        |  SSM Session Manager port-forward (6443) from your laptop
```

---

## 4. Repository layout

Create exactly this tree (files marked *stretch* can be stubs until their milestone).

```
pg-pulse/
├── AGENTS.md                         # this spec
├── README.md                         # quickstart: local (kind) and AWS
├── Makefile                          # all entry points (section 11)
├── rust-toolchain.toml
├── Cargo.toml                        # workspace: router, rules-engine (excludes extensions/pulse_health)
├── renovate.json
├── .editorconfig
├── .clang-format                     # match PostgreSQL style (tabs, BSD braces)
├── .gitignore                        # include *.tfstate*, .terraform/, *.tfvars (keep *.tfvars.example)
├── .env.example
├── docker-compose.yml                # local: pulse-main + shard-0 + shard-1 + router
├── docker-compose.chaos.yml          # chaos: debug/assert build, core dumps enabled
│
├── extensions/
│   ├── pulse_core/                   # C: query tracker
│   │   ├── Makefile                  # PGXS
│   │   ├── pulse_core.control
│   │   ├── pulse_core--0.1.0.sql
│   │   ├── src/
│   │   │   ├── pulse_core.c          # PG_MODULE_MAGIC, _PG_init, GUCs, hook install
│   │   │   ├── shmem.c / shmem.h     # shmem_request/startup hooks, hash table, LWLock tranche
│   │   │   ├── tracker.c / tracker.h # Executor{Start,Run,Finish,End} hooks, nesting level
│   │   │   ├── sqlapi.c              # pulse_queries(), pulse_reset()
│   │   │   └── chaos.c               # compiled ONLY when PULSE_CHAOS=1
│   │   └── test/
│   │       ├── pulse_core.conf       # shared_preload_libraries, compute_query_id=on
│   │       ├── sql/{basic,reset,gucs,nested}.sql
│   │       └── expected/{basic,reset,gucs,nested}.out
│   │
│   ├── pulse_decoder/                # C: logical decoding output plugin
│   │   ├── Makefile
│   │   ├── src/pulse_decoder.c
│   │   └── test/
│   │       ├── pulse_decoder.conf    # wal_level=logical, max_replication_slots
│   │       ├── sql/{insert_update_delete,toast,options}.sql
│   │       └── expected/*.out
│   │
│   └── pulse_health/                 # Rust pgrx: background worker
│       ├── Cargo.toml
│       ├── pulse_health.control
│       └── src/
│           ├── lib.rs                # _PG_init, GUCs, worker registration, SQL fns, schema via extension_sql!
│           ├── worker.rs             # main loop, latch, SIGHUP/SIGTERM
│           ├── alerts.rs             # open/dedupe/resolve alerts
│           └── checks/
│               ├── mod.rs            # Check trait, run_all()
│               ├── bloat.rs
│               ├── vacuum.rs
│               ├── xmin.rs
│               └── wal.rs
│
├── router/                           # stretch (M8)
│   ├── Cargo.toml
│   ├── config.example.yaml
│   ├── src/
│   │   ├── main.rs
│   │   ├── config.rs
│   │   ├── listener.rs               # accept loop, SSLRequest -> 'N'
│   │   ├── protocol/{mod,frontend,backend,codec}.rs
│   │   ├── routing.rs                # sqlparser: find shard key, hash -> shard
│   │   ├── pool.rs                   # per-shard backend pools, txn pooling
│   │   ├── session.rs                # per-client state machine
│   │   └── metrics.rs                # /metrics, /healthz, /readyz on :9090
│   └── tests/routing_it.rs           # testcontainers: two Postgres 18 shards
│
├── rules-engine/                     # stretch (M9)
│   ├── Cargo.toml
│   ├── src/{main,sandbox,poller}.rs
│   └── rules/example.ts
│
├── docker/
│   ├── postgres/Dockerfile           # builds all 3 extensions -> CNPG operand image
│   ├── postgres/Dockerfile.chaos     # PG 18 from source with --enable-cassert --enable-debug + PULSE_CHAOS=1
│   ├── router/Dockerfile
│   └── rules-engine/Dockerfile
│
├── deploy/
│   ├── argocd/
│   │   ├── install/
│   │   │   ├── kustomization.yaml    # upstream install.yaml pinned to v3.5.x + patches
│   │   │   ├── argocd-cm-patch.yaml  # health checks: Application + CNPG Cluster
│   │   │   └── small-node-patch.yaml # scale unused components to 0, set requests (section 8.7)
│   │   ├── projects/pg-pulse.yaml    # AppProject (allowed repos/destinations)
│   │   ├── root-app.yaml             # app-of-apps -> deploy/argocd/apps
│   │   └── apps/
│   │       ├── cnpg-operator.yaml    # wave -1, Helm chart, pinned
│   │       ├── pg-pulse-dev.yaml     # wave 0, path deploy/overlays/dev, automated
│   │       └── pg-pulse-prod.yaml    # wave 0, path deploy/overlays/prod, manual sync (not deployed on AWS by default)
│   ├── base/
│   │   ├── kustomization.yaml
│   │   ├── namespace.yaml
│   │   ├── kustomizeconfig/cnpg-images.yaml   # teach kustomize about spec.imageName
│   │   ├── db/cluster.yaml           # pulse-main
│   │   ├── shards/{shard-0,shard-1}.yaml
│   │   ├── router/{deployment,service,configmap,networkpolicy,pdb}.yaml
│   │   └── rules-engine/{deployment,configmap}.yaml   # stretch
│   └── overlays/
│       ├── ci/kustomization.yaml     # local tags, imagePullPolicy IfNotPresent, 1 instance
│       ├── dev/kustomization.yaml    # runs on AWS (or local kind); tags bumped by CI
│       └── prod/kustomization.yaml   # tags promoted by PR only
│
├── infra/
│   ├── bootstrap/                    # apply ONCE from your laptop
│   │   ├── main.tf                   # S3 state bucket, GitHub OIDC provider, CI IAM roles
│   │   ├── variables.tf
│   │   ├── outputs.tf                # role ARNs to paste into repo variables
│   │   └── terraform.tfvars.example
│   └── aws/                          # the environment (section 8.3)
│       ├── backend.tf
│       ├── providers.tf
│       ├── variables.tf
│       ├── network.tf
│       ├── security.tf
│       ├── iam.tf
│       ├── instance.tf
│       ├── schedule.tf
│       ├── budget.tf
│       ├── alarms.tf                 # optional idle auto-stop, off by default
│       ├── outputs.tf
│       ├── terraform.tfvars.example
│       └── templates/user-data.sh.tftpl
│
├── chaos/
│   ├── run-all.sh
│   ├── pgbench-load.sh
│   ├── kill9-backend.sh
│   ├── kill9-postmaster.sh
│   ├── trigger-crash.sql
│   ├── collect-core.sh               # gdb -batch -ex "bt full"
│   └── verify-recovery.sql           # amcheck + row counts + extension sanity
│
├── scripts/
│   ├── kind-config.yaml
│   ├── kind-up.sh
│   ├── argocd-bootstrap.sh           # install Argo CD, apply project + root app (used by kind AND EC2 user-data)
│   ├── e2e.sh                        # smoke tests (section 9.2)
│   ├── bump-image.sh                 # kustomize edit set image (used by CI)
│   ├── aws-power.sh                  # start | stop | status for the tagged instance
│   └── aws-tunnel.sh                 # fetch kubeconfig from SSM + SSM port-forward 6443
│
├── .github/workflows/
│   ├── ci.yml
│   ├── e2e.yml
│   ├── release.yml
│   ├── promote.yml
│   ├── chaos.yml
│   ├── infra.yml                     # terraform plan on PR, apply on main (approval gated)
│   └── aws-power.yml                 # manual start / stop / status
│
└── docs/
    ├── architecture.md
    ├── gucs.md                       # every GUC: name, type, default, context, meaning
    ├── runbook.md                    # alerts, crash debugging, slot replay, AWS pause/resume
    ├── aws-costs.md                  # the cost table from section 8.2, kept current
    └── decisions/0001-record-architecture-decisions.md
```

---

## 5. Component specs

### 5.1 `pulse_core` (C, PGXS)

**Load model:** must be in `shared_preload_libraries`. In `_PG_init`, if `!process_shared_preload_libraries_in_progress`, `ereport(ERROR)` with a clear hint.

**Shared memory**

- Use `shmem_request_hook` to call `RequestAddinShmemSpace()` and `RequestNamedLWLockTranche("pulse_core", 1)`.
- Use `shmem_startup_hook` to `ShmemInitStruct()` a header and `ShmemInitHash()` a hash table keyed by `queryId` (uint64).
- Chain to any previous hook values. Restore nothing on unload (PG does not unload preloaded libs).
- Entry: `queryid`, `calls`, `total_time_ms`, `min_time_ms`, `max_time_ms`, `rows`, `last_seen` (TimestampTz), plus a per-entry `slock_t` for counter updates.
- Locking: shared LWLock to read/update an existing entry (counters guarded by the entry spinlock), exclusive LWLock to insert or evict. Same pattern as `pg_stat_statements`.
- When full: evict the entry with the lowest `calls` under exclusive lock.

**Hooks**

- Install `ExecutorStart_hook`, `ExecutorRun_hook`, `ExecutorFinish_hook`, `ExecutorEnd_hook`. **Copy exact signatures from PG 18 `executor/executor.h`.**
- Track nesting level so only top-level statements count when `pulse.track = top` (also support `all` and `none`).
- In ExecutorStart, if `queryId != 0` and tracking, ensure `queryDesc->totaltime` is allocated in the query's memory context (mirror pg_stat_statements).
- In ExecutorEnd, accumulate time and `es_processed` rows.

**GUCs** (document all in `docs/gucs.md`)

Name
Type
Default
Context

`pulse.max_entries`
int (100..100000)
1000
PGC_POSTMASTER

`pulse.track`
enum `none/top/all`
`top`
PGC_SUSET

`pulse.chaos_crash`
bool
off
PGC_SUSET, **only defined when compiled with PULSE_CHAOS**

Call `MarkGUCPrefixReserved("pulse")`.

**SQL API** (`pulse_core--0.1.0.sql`, schema `pulse`)

- `pulse.queries()` returns `TABLE(queryid bigint, calls bigint, total_ms float8, mean_ms float8, min_ms float8, max_ms float8, rows bigint, last_seen timestamptz)` (SRF, materialize mode).
- `pulse.reset()` returns void, `pg_monitor`-gated via GRANT, not hardcoded checks.
- `REVOKE ALL ... FROM PUBLIC; GRANT EXECUTE ... TO pg_monitor;`

**Coding rules:** `palloc` in the right memory context, `ereport` not `exit`, no `malloc` in backends, no global state outside shmem except hook pointers and GUC variables. Build with `-Werror` in CI.

**Tests:** pg_regress via PGXS with a temp instance so the preload config applies:

```
REGRESS = basic reset gucs nested
REGRESS_OPTS = --temp-instance=./tmp_check --temp-config=test/pulse_core.conf --inputdir=test
```

Cases: queries are counted; `calls` increments on repeat; `reset()` clears; nested SQL function calls respect `pulse.track`; invalid GUC values are rejected. Normalize timing columns in tests (e.g. `total_ms > 0` instead of exact values).

### 5.2 `pulse_decoder` (C, logical decoding output plugin)

- Export `_PG_output_plugin_init(OutputPluginCallbacks *cb)` registering `startup`, `begin`, `change`, `commit`, `truncate`, `shutdown`.
- Output: one JSON object per change, one line each:

```
{"op":"I","schema":"public","table":"t","new":{"id":"1","v":"a"}}{"op":"U","schema":"public","table":"t","old":{"id":"1"},"new":{"id":"1","v":"b"}}{"op":"D","schema":"public","table":"t","old":{"id":"1"}}
```
- Options (parsed in `startup`): `include-xids` (bool, default false), `include-lsn` (bool, default false), `include-timestamp` (bool, default false). Keep defaults off so test output is deterministic.
- Values: convert with the type output function; JSON-escape with the helpers in `utils/json.h` (check PG 18 signatures). NULL becomes JSON `null`.
- Unchanged TOAST columns on UPDATE: emit `"__unchanged_toast__"` marker instead of reading the value.
- `old` keys depend on `REPLICA IDENTITY`; document this in the runbook.
- Allocate per-change data in a private memory context created in `startup`, reset after each change.
- **Tests:** temp instance with `wal_level=logical`; create slot with `pg_create_logical_replication_slot('s', 'pulse_decoder')`, run DML, read with `pg_logical_slot_get_changes('s', NULL, NULL)`; cover insert/update/delete, NULLs, special characters needing escaping, TOAST, truncate, and options.

### 5.3 `pulse_health` (Rust, pgrx 0.19.x)

- Separate crate (not in the root Cargo workspace; add it to `exclude`). Schema `pulse_health`.
- Reference: the bgworker example in the pgrx repo (`pgrx-examples/bgworker`) for the exact 0.19 API.
- `_PG_init`: register a static background worker with `BackgroundWorkerBuilder` (SPI access enabled, restart time 10 s). Requires `shared_preload_libraries`.
- Worker main: attach SIGHUP + SIGTERM handlers, connect to `pulse_health.database` (default `postgres`), loop on `wait_latch(interval)`. On SIGHUP reload config; on SIGTERM exit cleanly.
- Each iteration runs all checks inside one transaction via SPI and writes results through `alerts.rs`.

**Checks** (each implements a `Check` trait returning `Vec<Finding>`; thresholds are GUCs)

Check
Logic (use catalog/stat views, not guesses)

`bloat`
`pg_stat_user_tables`: `n_dead_tup > min_dead` and `n_dead_tup / (n_live_tup + 1) > ratio`

`vacuum`
dead tuples high and `last_autovacuum` older than N min; also `age(relfrozenxid)` above wraparound warning threshold

`xmin`
oldest `backend_xmin` in `pg_stat_activity`, sessions `idle in transaction` longer than N s, replication slots holding `xmin`/`catalog_xmin`

`wal`
total size from `pg_ls_waldir()` above threshold; per-slot retained WAL via `pg_wal_lsn_diff(pg_current_wal_lsn(), restart_lsn)`

**Alerts table** (created with `extension_sql!`)

```
CREATE TABLE pulse_health.alerts (
  id          bigserial PRIMARY KEY,
  check_name  text        NOT NULL,
  object      text        NOT NULL,
  severity    text        NOT NULL CHECK (severity IN ('info','warning','critical')),
  detail      jsonb       NOT NULL,
  opened_at   timestamptz NOT NULL DEFAULT now(),
  resolved_at timestamptz
);
CREATE UNIQUE INDEX alerts_one_open ON pulse_health.alerts (check_name, object) WHERE resolved_at IS NULL;
```

- Dedupe: upsert against the open alert; resolve (`resolved_at = now()`) when a finding disappears.
- SQL function `pulse_health.run_once()` runs all checks synchronously and returns the findings (used by tests and e2e so they do not wait on the worker).

**GUCs:** `pulse_health.interval_s` (default 30), `pulse_health.database`, `pulse_health.bloat_ratio`, `pulse_health.bloat_min_dead`, `pulse_health.idle_txn_s`, `pulse_health.wal_bytes_warn`, `pulse_health.xid_age_warn`.

**Tests (`#[pg_test]`):** for each check, craft the condition (e.g. table with `autovacuum_enabled = off`, delete rows, call `pg_stat_force_next_flush()`), call `run_once()`, assert the finding; then fix the condition and assert the alert resolves. Add a test that the worker is registered (`pg_stat_activity.backend_type`).

### 5.4 `router` (Rust, tokio) *stretch, M8*

- Listen on `:6432`. Answer `SSLRequest` with `'N'`. Parse `StartupMessage`.
- Use the `postgres-protocol` crate for message encoding and the SCRAM-SHA-256 client to authenticate **to** backends with the CNPG-generated `<cluster>-app` secret. Client-side auth for MVP: trust, restricted by NetworkPolicy. Stretch: SCRAM server.
- **Simple query protocol only** for MVP; reject extended protocol (`Parse`) with a clear `ErrorResponse` (SQLSTATE `0A000`).
- Routing: parse with the `sqlparser` crate (Postgres dialect); find an equality predicate on the configured shard column (`tenant_id = <int literal>`) or an `INSERT` value for it; shard = stable hash (xxhash64) of the key mod N. No key: return an error (MVP). Stretch: scatter-gather read-only `SELECT` with row merging.
- Transaction pooling: a client holds a backend connection only while `ReadyForQuery` status is `T` or `E`. Pin the shard for the transaction; a statement targeting a different shard inside the same transaction returns an error (no distributed transactions).
- Per-shard pool size, timeouts, and shard endpoints from YAML config (ConfigMap); credentials from env (Secret).
- `/metrics` (Prometheus): queries by shard, pool in-use/idle, errors by SQLSTATE, routing latency histogram. `/healthz`, `/readyz` (ready only when every shard pool has at least one healthy conn).
- Tests: unit tests for codec and routing; integration tests with testcontainers spinning up two `postgres:18` shards.
- Shard schema bootstrap: each shard Cluster uses `bootstrap.initdb.postInitApplicationSQL` to create `orders(tenant_id int, id bigint, amount numeric, created_at timestamptz default now(), primary key (tenant_id, id))`.

### 5.5 `rules-engine` (Rust + deno_core) *stretch, M9*

- Polls `pulse_health.alerts` for new open alerts. Loads `*.ts` rules from a mounted ConfigMap. Each rule exports `function evaluate(alert): {notify: boolean, message: string}`.
- Each evaluation runs in a fresh isolate with a heap limit (V8 heap limits in the runtime create params) and a CPU budget enforced by a watchdog thread calling `terminate_execution` after N ms. Rule failures are logged, never crash the service.
- Output: structured JSON log line (stretch: webhook).

---

## 6. Containers

### 6.1 `docker/postgres/Dockerfile` (deployable)

- Stage `builder`: `FROM postgres:${PG_MAJOR}-trixie`; apt install `postgresql-server-dev-${PG_MAJOR}`, build tools, `libclang-dev`, Rust (rustup, pinned toolchain), `cargo-pgrx` at the pinned version. Build `pulse_core` and `pulse_decoder` with PGXS; build `pulse_health` with `cargo pgrx package --pg-config $(which pg_config)`.
- Stage `runtime`: `FROM ghcr.io/cloudnative-pg/postgresql:${PG_VERSION}-standard-trixie`. Copy `.so` files to `$(pg_config --pkglibdir)` and control/SQL files to `$(pg_config --sharedir)/extension`. Keep the base image's non-root postgres user (verify UID, CNPG uses 26).
- Must build natively on both `linux/amd64` and `linux/arm64` (no cross-compile tricks; CI uses native runners, section 9.4).
- Labels: OCI source, revision, version. Release builds must **not** contain the `pulse.chaos_crash` GUC (CI asserts this with `strings`/`nm`).

### 6.2 `docker/postgres/Dockerfile.chaos` (never deployed)

- Build PostgreSQL 18 from source with `--enable-debug --enable-cassert CFLAGS="-O0 -g"` so freed memory is clobbered (makes use-after-free crash deterministically).
- Build `pulse_core` with `PULSE_CHAOS=1`, which compiles `chaos.c`: when `pulse.chaos_crash = on`, `pulse.reset()` stores a pointer inside a palloc'd struct, `pfree`s the struct, then dereferences the stored pointer. This is the planted bug for core dump practice.
- amd64 only. Tag: `:<sha>-chaos`. Only `chaos.yml` and `docker-compose.chaos.yml` use it.

### 6.3 `router` and `rules-engine` images

- Rust builder stage, runtime `gcr.io/distroless/cc-debian12` (or newer distroless matching the builder's glibc), non-root, read-only root filesystem. amd64 + arm64.

---

## 7. Kubernetes and Argo CD (GitOps)

### 7.1 Bootstrap

- `make kind-up`: create kind cluster from `scripts/kind-config.yaml`.
- `make argocd-up`: runs `scripts/argocd-bootstrap.sh` against the current kube context: `kubectl apply -k deploy/argocd/install`, wait for rollout, apply `deploy/argocd/projects/`, then `deploy/argocd/root-app.yaml`; print the initial admin password and a port-forward command.
- The **same script** is called by the EC2 user-data on AWS (section 8.4). Keep it idempotent and non-interactive.
- If the repo or GHCR packages are private, the script creates the Argo CD repo secret and an image pull secret from tokens (from `.env` locally, from SSM Parameter Store on AWS). Never commit them.

### 7.2 Argo CD config

- `install/argocd-cm-patch.yaml` adds two custom health checks:

- `argoproj.io/Application`: report the child app's health so **app-level sync waves actually wait** (use the snippet from the Argo CD docs).
- `postgresql.cnpg.io/Cluster`: Healthy when `status.phase` equals the CNPG healthy phase string (verify the exact string in the CNPG API reference), Progressing otherwise, Degraded on failure phases.
- `install/small-node-patch.yaml`: see section 8.7.
- `projects/pg-pulse.yaml`: AppProject limited to this repo, the CNPG Helm repo, and namespaces `cnpg-system`, `pg-pulse-dev`, `pg-pulse-prod`.
- `root-app.yaml`: app-of-apps over `deploy/argocd/apps`, automated with prune + selfHeal.
- `apps/cnpg-operator.yaml`: Helm source (CloudNativePG chart, pinned version), wave `-1`, `ServerSideApply=true` (large CRDs).
- `apps/pg-pulse-dev.yaml`: kustomize path `deploy/overlays/dev`, wave `0`, automated prune + selfHeal, `CreateNamespace=true`.
- `apps/pg-pulse-prod.yaml`: path `deploy/overlays/prod`, **no automated sync** (manual promotion). Excluded from the AWS root app by default (an `aws` flag in the bootstrap script) so the single small node only runs dev.

### 7.3 Manifests

- Resource-level waves inside each overlay: Clusters `0`, router `1`, rules-engine `2`. Add `argocd.argoproj.io/sync-options: SkipDryRunOnMissingResource=true` to CNPG `Cluster` resources.
- `base/db/cluster.yaml` (shape, fill in exact values):

```
apiVersion: postgresql.cnpg.io/v1kind: Clustermetadata:  name: pulse-main  annotations:    argocd.argoproj.io/sync-wave: "0"spec:  instances: 3  imageName: ghcr.io/OWNER/pg-pulse-postgres:latest   # overridden per overlay  postgresql:    shared_preload_libraries: [pulse_core, pulse_health]    parameters:      compute_query_id: "on"      pulse.max_entries: "1000"      pulse_health.interval_s: "30"      # ensure wal_level is logical (check CNPG default; set it if needed)  storage:    size: 5Gi    # no storageClass: uses the cluster default (kind "standard", k3s "local-path")
```
- **Kustomize image override for CRDs:** the built-in `images:` transformer does not know `spec.imageName`. Add `base/kustomizeconfig/cnpg-images.yaml`:

```
images:  - path: spec/imageName    kind: Cluster
```
and reference it from `base/kustomization.yaml` under `configurations:`. Verify with `kustomize build deploy/overlays/dev | grep imageName`.
- Router: Deployment, Service, ConfigMap, NetworkPolicy (only pods labeled `app: pg-pulse-client` may reach 6432), PodDisruptionBudget, resource requests/limits, probes on `/healthz` and `/readyz`.
- Overlays:

- `ci`: 1 instance per cluster, local tags, `imagePullPolicy: IfNotPresent`.
- `dev` (**runs on the AWS node**): 1 instance per cluster, 2Gi storage each, router 1 replica, small requests sized to fit the memory budget in section 8.7, `shared_buffers: 128MB`, tags bumped by CI.
- `prod`: 3 instances, 2 router replicas, PR-promoted tags. Practice HA locally on kind or on a larger instance.
- Note for the runbook: changing `shared_preload_libraries` needs a restart; CNPG performs a rolling restart, primary last.

---

## 8. AWS deployment (cheap and pausable)

### 8.1 Design rules

- **One Graviton EC2 instance running k3s** hosts the `dev` environment: Argo CD, CNPG, all three Postgres clusters, router, rules-engine. Everything in section 7 runs on it unchanged.
- **Allowed paid resources:** the EC2 instance, its gp3 root volume, its auto-assigned public IPv4 (billed only while running), one S3 bucket for Terraform state, one AWS Budget, one EventBridge Scheduler schedule, and (optional, off by default) one CloudWatch alarm.
- **Forbidden without an approved ADR:** EKS, NAT gateways, load balancers, Elastic IPs, VPC interface endpoints, RDS, ECR, CloudWatch Logs, detailed monitoring, anything multi-AZ.
- **No inbound ports.** Access is only through SSM Session Manager port forwarding.
- Images stay on GHCR. CI e2e and chaos stay on kind inside GitHub Actions, so pull requests never spend AWS money.
- Every resource carries `default_tags { Project = "pg-pulse", Env = "dev", ManagedBy = "terraform" }`.

### 8.2 Cost model (us-east-1, on-demand, verify current prices into `docs/aws-costs.md`)

Item
Rate
If left on 24/7
While paused

EC2 t4g.large (2 vCPU, 8 GiB) on-demand
$0.0672/hr
~$49/mo
$0

EC2 t4g.large spot (optional)
~$0.024/hr
~$17/mo
$0

Public IPv4 (auto-assigned, not an EIP)
$0.005/hr
~$3.65/mo
$0

gp3 root volume, 30 GB
$0.08/GB-month
$2.40/mo
$2.40/mo

*For comparison, not used:* EKS control plane
$0.10/hr
~$73/mo
$73/mo

*For comparison, not used:* NAT gateway
$0.045/hr + data
~$33/mo
$33/mo

- Typical study use, 4 hours per weekday (~88 hrs/month): **~$8.75/month** on-demand, **~$5/month** on spot.
- Paused: **~$2.40/month**. Destroyed: **$0**.
- `instance_type` is a variable. `t4g.medium` (4 GiB) roughly halves compute cost but only fits with the trimmed Argo CD and tight requests; document the tradeoff in an ADR if used.

### 8.3 Terraform (`infra/`)

**`infra/bootstrap/`** (applied once, locally, with your own AWS credentials; local state is fine here):

- S3 bucket for Terraform state: versioning on, SSE-S3, block all public access, lifecycle expiring noncurrent versions after 30 days.
- GitHub OIDC provider (`token.actions.githubusercontent.com`).
- IAM roles for GitHub Actions (trust policies pinned to this repo):

Role
Trusted subject
Permissions

`pg-pulse-gha-plan`
`repo:OWNER/REPO:pull_request`
`ReadOnlyAccess` + read/write the state bucket lock file only

`pg-pulse-gha-apply`
`repo:OWNER/REPO:environment:aws`
Scoped to the services in 8.3 (EC2, VPC, IAM for the instance role/profile and scheduler role, SSM params under `/pg-pulse/*`, Scheduler, Budgets, CloudWatch alarms, state bucket). **No `AdministratorAccess`.**

`pg-pulse-gha-power`
`repo:OWNER/REPO:ref:refs/heads/main`
`ec2:StartInstances`, `ec2:StopInstances` only where `aws:ResourceTag/Project = pg-pulse`; `ec2:DescribeInstances`

**`infra/aws/`** (the environment):

- `backend.tf`: S3 backend, `use_lockfile = true`, key `pg-pulse/dev.tfstate`.
- `variables.tf`: `region` (default `us-east-1`), `instance_type` (default `t4g.large`), `use_spot` (default `false`), `root_volume_gb` (default 30), `k3s_version`, `argocd_version`, `repo_url`, `git_ref` (default `main`), `auto_stop_cron` (default `cron(0 2 * * ? *)`), `auto_stop_timezone` (default `UTC`, user should set theirs), `budget_usd` (default 15), `budget_email` (no default, set in untracked `terraform.tfvars`), `enable_idle_stop_alarm` (default `false`).
- `network.tf`: VPC `10.42.0.0/16`, **one** public subnet, internet gateway, route table. `map_public_ip_on_launch = true`. **No NAT gateway.**
- `security.tf`: security group with **zero ingress rules**, all egress.
- `iam.tf`: instance role + profile with `AmazonSSMManagedInstanceCore`, plus `ssm:GetParameter`/`PutParameter` on `parameter/pg-pulse/*` (add KMS permissions for the `aws/ssm` key only if required). Scheduler role allowed `ec2:StopInstances` on this instance only.
- `instance.tf`:

- AMI from the Canonical SSM public parameter for Ubuntu 24.04 arm64 gp3 (verify the exact parameter path).
- `metadata_options { http_tokens = "required" }` (IMDSv2).
- Root volume gp3, encrypted, `root_volume_gb`.
- `credit_specification { cpu_credits = "standard" }` so burst never adds charges (T4g defaults to unlimited).
- Basic monitoring only.
- If `use_spot`: `instance_market_options` with `spot_instance_type = "persistent"` and `instance_interruption_behavior = "stop"`, so an interruption behaves like a pause and data survives.
- `user_data` from `templates/user-data.sh.tftpl`; `user_data_replace_on_change = false` (changing the script must not silently replace the box; use `make aws-recreate`).
- `schedule.tf`: EventBridge Scheduler schedule using the universal target for `ec2:StopInstances` at `auto_stop_cron` in `auto_stop_timezone`.
- `budget.tf`: monthly cost budget `budget_usd` with email notifications at 50%, 80%, and 100% (forecasted and actual) to `budget_email`.
- `alarms.tf`: if `enable_idle_stop_alarm`, a CloudWatch alarm on `CPUUtilization` below a threshold for 2 hours with the EC2 stop action. Off by default because an idle k3s node is never at 0% CPU; tune the threshold before enabling.
- `outputs.tf`: `instance_id`, `region`, and ready-to-paste `make aws-tunnel` / `aws-power` commands.
- Validation: `terraform plan` must never contain `aws_nat_gateway`, `aws_eip`, `aws_lb`, `aws_eks_cluster`, `aws_vpc_endpoint`, `aws_db_instance`, or `aws_ecr_repository`. CI enforces this (section 9.6).

### 8.4 Instance setup (`user-data.sh.tftpl`, first boot only)

1. Install pinned k3s with `--disable traefik --disable servicelb --secrets-encryption --write-kubeconfig-mode 600`. Keep the local-path provisioner (default StorageClass) and metrics-server.
2. If SSM SecureString parameters `/pg-pulse/github-token` or `/pg-pulse/ghcr-token` exist, export them for the bootstrap script (repo secret + image pull secret). The user creates these with the AWS CLI; Terraform never sees them.
3. Clone the repo at `git_ref` and run `scripts/argocd-bootstrap.sh --env aws` (dev only, small-node patch on).
4. Copy the kubeconfig, rewrite the server to `https://127.0.0.1:6443`, and store it in SSM SecureString `/pg-pulse/kubeconfig`.
5. Log to `/var/log/pg-pulse-bootstrap.log`. Every step must be idempotent so a manual rerun is safe.
6. On later boots nothing in user-data reruns; systemd starts k3s, CNPG brings Postgres back, and Argo CD catches up with git on its next poll.

### 8.5 Access

- `make aws-tunnel` (`scripts/aws-tunnel.sh`): fetch `/pg-pulse/kubeconfig` from SSM into `~/.kube/pg-pulse-aws`, then run `aws ssm start-session --target <id> --document-name AWS-StartPortForwardingSession --parameters portNumber=6443,localPortNumber=6443`.
- With the tunnel up, plain `kubectl` works, and `kubectl -n argocd port-forward svc/argocd-server 8080:443` opens the Argo CD UI.
- Requires the AWS CLI Session Manager plugin locally (document install in README).
- The public IP changes on every start. Nothing depends on it.

### 8.6 Pause and resume

Action
How
Result

Pause
`make aws-pause`, or run `aws-power.yml` with `stop`
Instance stops; Postgres data and k3s state stay on EBS. Cost drops to ~$2.40/mo.

Resume
`make aws-resume`, or `aws-power.yml` with `start`
Everything returns in a few minutes; Argo CD syncs anything merged while paused.

Status
`make aws-status`, or `aws-power.yml` with `status`
Instance state, launch time, uptime this boot.

Destroy
`make aws-destroy`
`terraform destroy` of `infra/aws`; $0, all data gone. Bootstrap stack stays.

Recreate
`make aws-recreate`
Destroy + apply; Argo CD rebuilds everything from git.

- `aws-power.yml` runs from the GitHub Actions tab in any browser, including a phone, so pausing never needs a laptop.
- Safety nets on by default: nightly auto-stop schedule and the budget email alerts. `make aws-status` warns if the instance has been up longer than 8 hours.
- Optional script `scripts/aws-power.sh status --cost` may call Cost Explorer, but note each Cost Explorer API request is billed; keep it opt-in.

### 8.7 Fitting on one small node

- `install/small-node-patch.yaml` scales `argocd-dex-server`, `argocd-notifications-controller`, and `argocd-applicationset-controller` to 0 replicas (unused), and sets modest requests/limits on the repo-server, application-controller, server, and redis.
- Target memory budget on 8 GiB: k3s ~0.5 GiB, Argo CD ~0.8 GiB, CNPG operator ~0.2 GiB, three Postgres instances ~0.4 GiB each, router + rules-engine ~0.3 GiB, leaving headroom for the OS and page cache. Set requests to match; verify with `kubectl top`.
- Single-node means no real HA on AWS. That is intentional for cost. HA and failover drills happen on kind.

---

## 9. CI/CD (GitHub Actions)

### 9.1 `ci.yml` (on pull_request and push)

Jobs (run in parallel where possible, cache `~/.cargo`, `target/`, and `~/.pgrx`):

1. **lint**: `clang-format --dry-run --Werror`, `cppcheck`, `cargo fmt --check`, `cargo clippy -- -D warnings` (workspace and pulse_health), `hadolint`, `shellcheck`, `yamllint`.
2. **test-c**: install `postgresql-${PG_MAJOR}` + server dev from PGDG; `make -C extensions/pulse_core installcheck` and same for `pulse_decoder`; upload `regression.diffs` on failure.
3. **test-pgrx**: `cargo pgrx init --pg${PG_MAJOR} download` (cached), `cargo pgrx test pg${PG_MAJOR}`.
4. **test-router** *(from M8)*: `cargo test -p router` including testcontainers integration tests.
5. **manifests**: `kustomize build` every overlay and `deploy/argocd/install`; validate with `kubeconform -strict` using CRD schemas from the datreeio CRDs catalog (CNPG Cluster, Argo CD Application/AppProject).
6. **images**: `docker buildx build` all images for amd64 only (no push on PRs); `trivy image --exit-code 1 --severity CRITICAL`; assert release postgres image has no `chaos_crash` symbol.

### 9.2 `e2e.yml` (on PRs labeled `e2e` and on push to main)

- Create kind cluster, `helm install` CNPG (pinned), build images, `kind load docker-image`, `kubectl apply -k deploy/overlays/ci`, wait for Clusters healthy.
- `scripts/e2e.sh` asserts:

- `CREATE EXTENSION pulse_core; CREATE EXTENSION pulse_health;` succeed; `pulse.queries()` returns rows after a few queries.
- `pulse_health.run_once()` reports a bloat finding on a crafted table.
- A logical slot with `pulse_decoder` returns the expected JSON for an INSERT.
- *(M8+)* through the router: `INSERT ... tenant_id = 1` and `= 2` land on the shards predicted by the hash function.
- Runs entirely on the GitHub runner. **No AWS calls.** Argo CD manifests are validated in `ci.yml`.

### 9.3 `release.yml` (on push to main after CI passes, and on `v*` tags)

1. Build images natively per architecture (section 9.4), push to GHCR by digest, merge into tags `sha-<short>` (and `vX.Y.Z` on tags) using the GitHub OIDC token (no PATs).
2. Sign with cosign keyless; attach an SBOM (syft).
3. Run `scripts/bump-image.sh dev sha-<short>` (`kustomize edit set image` in `deploy/overlays/dev`), commit as the bot with `[skip ci]`, push. Commits made with `GITHUB_TOKEN` do not retrigger workflows, which prevents loops.
4. Argo CD on the EC2 node picks up the change on its next poll. If the instance is paused, it syncs on resume.

### 9.4 Multi-arch builds

- Matrix over `ubuntu-24.04` (amd64) and `ubuntu-24.04-arm` (arm64). Both are standard GitHub-hosted runners and work in private repos (they count against your plan's minutes).
- Each leg builds and pushes by digest; a final job runs `docker buildx imagetools create` to publish one multi-arch tag.
- **Do not** build the Rust/pgrx images under QEMU emulation; it is far too slow.

### 9.5 `promote.yml` (workflow_dispatch, input: tag)

- Copies the chosen tag into `deploy/overlays/prod` and opens a PR. Merging it plus a manual sync in Argo CD deploys prod (wherever prod is enabled).

### 9.6 `infra.yml`

- **On PRs touching `infra/`:** `terraform fmt -check`, `terraform validate`, `tflint`, `trivy config infra/`, then `terraform plan` with `pg-pulse-gha-plan`; post the plan as a PR comment.
- **Cost guard:** fail the job if the plan JSON (`terraform show -json`) contains any forbidden resource type from section 8.3.
- **On push to main:** `terraform apply` with `pg-pulse-gha-apply`, inside the GitHub environment `aws` with **required reviewer** (you), so nothing in AWS changes without a tap to approve.
- Optional: Infracost comment on PRs (free tier, needs an API key secret).

### 9.7 `aws-power.yml` (workflow_dispatch, input `action`: start | stop | status)

- Assumes `pg-pulse-gha-power`, finds the instance by tag `Project=pg-pulse`, runs the action, prints state in the job summary.

### 9.8 `chaos.yml` (nightly cron + workflow_dispatch, runs on GitHub runners only)

- Set `kernel.core_pattern` on the runner to a mounted dir; compose sets `ulimits: core: -1`.
- `docker compose -f docker-compose.chaos.yml up` with the `-chaos` image, then `chaos/run-all.sh`:

1. `pgbench-load.sh`: 16 clients, 60 s, while `pulse.queries()` is polled; assert no crash.
2. `kill9-backend.sh`: `kill -9` a busy backend; assert postmaster crash-recovery completes, `pulse.queries()` works again (stats reset is expected), and the `pulse_health` worker restarts within 15 s.
3. `kill9-postmaster.sh`: kill mid-write, restart container, run `verify-recovery.sql` (`amcheck` on heap and btree, row counts consistent with committed pgbench transactions).
4. `trigger-crash.sql`: `SET pulse.chaos_crash = on; SELECT pulse.reset();`, expect the backend to crash.
5. `collect-core.sh`: `gdb -batch -ex "bt full"` on the newest core; **job passes only if the backtrace contains the planted function in `chaos.c`**. Upload the backtrace as an artifact.

### 9.9 Repo settings (document in README)

- Branch protection on `main`: require `lint`, `test-c`, `test-pgrx`, `manifests`, `images`, and `infra-plan` (when `infra/` changes).
- GitHub environment `aws` with you as required reviewer.
- Repository variables (not secrets): `AWS_REGION`, `AWS_ROLE_PLAN_ARN`, `AWS_ROLE_APPLY_ARN`, `AWS_ROLE_POWER_ARN`, `TF_STATE_BUCKET`.
- GHCR package visibility public (simplest) or private with the pull secret flow from 7.1.

---

## 10. Milestones and acceptance criteria

#
Milestone
Done when

M0
Scaffold
Tree from section 4 exists; empty extensions build; `make lint` passes; `ci.yml` green.

M1
`pulse_core`
All pg_regress tests pass; 60 s pgbench at 16 clients with no errors; `docs/gucs.md` complete.

M2
`pulse_health`
All `#[pg_test]` pass; every check fires and resolves; SIGTERM exits cleanly; worker visible in `pg_stat_activity`.

M3
`pulse_decoder`
Regression output matches for I/U/D, NULLs, escaping, TOAST, truncate, options.

M4
Containers + compose
`docker compose up` gives a DB where all three extensions load; images build on amd64 and arm64; scan clean.

M5
Kubernetes + Argo CD (kind)
`make kind-up && make argocd-up` results in all apps Synced + Healthy locally; `e2e.yml` green.

M6
AWS (pausable)
`make aws-bootstrap` then `make aws-up` creates the stack; via `make aws-tunnel`, Argo CD on EC2 shows all apps Synced + Healthy; a merge to main reaches EC2 with no manual step; `make aws-pause` then `make aws-resume` returns everything Healthy with Postgres data intact; `aws-power.yml` works; nightly stop fires; budget alert email received; `infra.yml` cost guard passes; `docs/aws-costs.md` filled in.

M7
Chaos
`chaos.yml` green, backtrace artifact shows the planted bug.

M8
Router *(stretch)*
Integration + e2e routing assertions pass; metrics exposed; transaction pinning enforced; deployed on EC2.

M9
Rules engine *(stretch)*
A rule exceeding its CPU or heap budget is terminated and logged while the service keeps running.

---

## 11. Makefile targets

```
make build          # all extensions + rust crates
make test           # installcheck (C) + pgrx test + cargo test
make lint           # everything in ci.yml lint job
make image          # build deployable images (PG_MAJOR, TAG vars), host arch
make image-chaos    # build chaos image
make up / down      # docker compose local stack
make kind-up        # create kind cluster
make argocd-up      # install Argo CD + root app on current kube context
make dev-deploy     # inner loop: build, kind load, kubectl apply -k deploy/overlays/ci (no Argo)
make e2e            # scripts/e2e.sh against current kube context
make chaos          # chaos suite locally

make aws-bootstrap  # one time: terraform apply infra/bootstrap
make aws-up         # terraform apply infra/aws
make aws-tunnel     # kubeconfig from SSM + port-forward 6443 over SSM
make aws-pause      # stop the instance
make aws-resume     # start the instance
make aws-status     # state + uptime (warns if up > 8h)
make aws-recreate   # destroy + apply infra/aws
make aws-destroy    # terraform destroy infra/aws ($0)
make clean
```

---

## 12. Guardrails

- Do not vendor PostgreSQL source into the repo (the chaos Dockerfile downloads the pinned tarball and verifies its checksum).
- Every GUC has a sane default, bounds, and a doc entry.
- Every SQL function has explicit `GRANT`/`REVOKE`.
- No `unwrap()` on fallible paths in Rust service code; errors map to proper SQLSTATEs in the router.
- Keep C files under ~500 lines each; split by responsibility as in the tree.
- AWS: least-privilege IAM, no inbound ports, no long-lived AWS keys in GitHub (OIDC only), no secrets in Terraform state, every resource tagged, forbidden resource types blocked in CI.
- If CI is red, fix it before starting new work.
