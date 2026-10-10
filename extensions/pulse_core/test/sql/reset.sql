SELECT pulse.reset();
SELECT count(*) = 1 AS reset_cleared_previous_entries FROM pulse.queries();
