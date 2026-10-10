CREATE OR REPLACE FUNCTION pulse.test_nested() RETURNS integer
LANGUAGE plpgsql
AS $$
BEGIN
	RETURN (SELECT 42);
END;
$$;

SET pulse.track = top;
SELECT pulse.reset();
SELECT pulse.test_nested();
SELECT count(*) = 2 AS top_level_only FROM pulse.queries();

SET pulse.track = 'all';
SELECT pulse.reset();
SELECT pulse.test_nested();
SELECT count(*) = 3 AS nested_statements_tracked FROM pulse.queries();
RESET pulse.track;
SELECT 1;
