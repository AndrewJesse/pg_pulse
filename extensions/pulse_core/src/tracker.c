#include "postgres.h"
#include "executor/executor.h"
#include "executor/instrument.h"
#include "utils/memutils.h"

#include "shmem.h"
#include "tracker.h"

static int nesting_level = 0;
static ExecutorStart_hook_type prev_ExecutorStart = NULL;
static ExecutorRun_hook_type prev_ExecutorRun = NULL;
static ExecutorFinish_hook_type prev_ExecutorFinish = NULL;
static ExecutorEnd_hook_type prev_ExecutorEnd = NULL;

static bool pulse_tracking_enabled(void)
{
	return pulse_track == 2 || (pulse_track == 1 && nesting_level == 0);
}

static void pulse_ExecutorStart(QueryDesc *queryDesc, int eflags)
{
	if (prev_ExecutorStart)
		prev_ExecutorStart(queryDesc, eflags);
	else
		standard_ExecutorStart(queryDesc, eflags);

	if (pulse_tracking_enabled() && queryDesc->plannedstmt->queryId != 0 &&
		queryDesc->totaltime == NULL) {
		MemoryContext old_context = MemoryContextSwitchTo(queryDesc->estate->es_query_cxt);
		queryDesc->totaltime = InstrAlloc(1, INSTRUMENT_ALL, false);
		MemoryContextSwitchTo(old_context);
	}
}

static void pulse_ExecutorRun(QueryDesc *queryDesc, ScanDirection direction, uint64 count)
{
	nesting_level++;
	PG_TRY();
	{
		if (prev_ExecutorRun)
			prev_ExecutorRun(queryDesc, direction, count);
		else
			standard_ExecutorRun(queryDesc, direction, count);
	}
	PG_FINALLY();
	{
		nesting_level--;
	}
	PG_END_TRY();
}

static void pulse_ExecutorFinish(QueryDesc *queryDesc)
{
	nesting_level++;
	PG_TRY();
	{
		if (prev_ExecutorFinish)
			prev_ExecutorFinish(queryDesc);
		else
			standard_ExecutorFinish(queryDesc);
	}
	PG_FINALLY();
	{
		nesting_level--;
	}
	PG_END_TRY();
}

static void pulse_ExecutorEnd(QueryDesc *queryDesc)
{
	uint64 queryid = queryDesc->plannedstmt->queryId;

	if (queryid != 0 && queryDesc->totaltime != NULL && pulse_tracking_enabled()) {
		InstrEndLoop(queryDesc->totaltime);
		pulse_record_query(queryid, queryDesc->totaltime->total * 1000.0,
						   queryDesc->estate->es_total_processed);
	}

	if (prev_ExecutorEnd)
		prev_ExecutorEnd(queryDesc);
	else
		standard_ExecutorEnd(queryDesc);
}

void pulse_install_executor_hooks(void)
{
	prev_ExecutorStart = ExecutorStart_hook;
	ExecutorStart_hook = pulse_ExecutorStart;
	prev_ExecutorRun = ExecutorRun_hook;
	ExecutorRun_hook = pulse_ExecutorRun;
	prev_ExecutorFinish = ExecutorFinish_hook;
	ExecutorFinish_hook = pulse_ExecutorFinish;
	prev_ExecutorEnd = ExecutorEnd_hook;
	ExecutorEnd_hook = pulse_ExecutorEnd;
}
