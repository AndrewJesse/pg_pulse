# GUC reference

## pulse_core

| Name                | Type                        | Default | Context          | Meaning                                                                                 |
| ------------------- | --------------------------- | ------- | ---------------- | --------------------------------------------------------------------------------------- |
| `pulse.max_entries` | integer (100..100000)       | `1000`  | `PGC_POSTMASTER` | Maximum number of query IDs kept in shared memory.                                      |
| `pulse.track`       | enum (`none`, `top`, `all`) | `top`   | `PGC_SUSET`      | Selects whether no statements, top-level statements, or nested statements are recorded. |
| `pulse.chaos_crash` | boolean                     | `off`   | `PGC_SUSET`      | Enables the planted use-after-free crash; defined only in chaos builds.                 |

## pulse_health

| Name                          | Type    | Default      | Context      | Meaning                                                   |
| ----------------------------- | ------- | ------------ | ------------ | --------------------------------------------------------- |
| `pulse_health.interval_s`     | integer | `30`         | `PGC_SIGHUP` | Seconds between background health checks.                 |
| `pulse_health.database`       | string  | `postgres`   | `PGC_SIGHUP` | Database used by the health worker.                       |
| `pulse_health.bloat_ratio`    | float   | `0.2`        | `PGC_SIGHUP` | Dead-to-live tuple ratio that triggers a bloat finding.   |
| `pulse_health.bloat_min_dead` | integer | `1000`       | `PGC_SIGHUP` | Minimum dead tuples before the bloat ratio is considered. |
| `pulse_health.idle_txn_s`     | integer | `600`        | `PGC_SIGHUP` | Idle-in-transaction age threshold in seconds.             |
| `pulse_health.wal_bytes_warn` | integer | `1073741824` | `PGC_SIGHUP` | WAL retention threshold in bytes.                         |
| `pulse_health.xid_age_warn`   | integer | `200000000`  | `PGC_SIGHUP` | Transaction ID age warning threshold.                     |
