CREATE SCHEMA IF NOT EXISTS pulse;
CREATE FUNCTION pulse.queries()
RETURNS TABLE (
  queryid bigint,
  calls bigint,
  total_ms float8,
  mean_ms float8,
  min_ms float8,
  max_ms float8,
  rows bigint,
  last_seen timestamptz
)
AS '$libdir/pulse_core', 'pulse_queries'
LANGUAGE C STABLE;

CREATE FUNCTION pulse.reset()
RETURNS void
AS '$libdir/pulse_core', 'pulse_reset'
LANGUAGE C;

REVOKE ALL ON FUNCTION pulse.queries() FROM PUBLIC;
REVOKE ALL ON FUNCTION pulse.reset() FROM PUBLIC;
GRANT EXECUTE ON FUNCTION pulse.queries() TO pg_monitor;
GRANT EXECUTE ON FUNCTION pulse.reset() TO pg_monitor;
