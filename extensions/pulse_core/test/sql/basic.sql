CREATE EXTENSION pulse_core;
SELECT 1;
SELECT 1;
SELECT count(*) > 0 AS has_entries FROM pulse.queries();
SELECT bool_or(calls >= 2) AS repeated_query FROM pulse.queries();
SELECT bool_or(rows > 0 AND total_ms > 0 AND last_seen IS NOT NULL AND min_ms <= mean_ms AND mean_ms <= max_ms) AS complete_stats FROM pulse.queries();
