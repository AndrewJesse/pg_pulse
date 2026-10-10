#include "postgres.h"
#include "fmgr.h"
#include "funcapi.h"
#include "utils/timestamp.h"
#include "utils/tuplestore.h"

#include "shmem.h"

PG_FUNCTION_INFO_V1(pulse_queries);
PG_FUNCTION_INFO_V1(pulse_reset);

Datum pulse_queries(PG_FUNCTION_ARGS)
{
	ReturnSetInfo *rsinfo;
	HASH_SEQ_STATUS status;
	PulseEntry *entry;

	if (pulse_shared == NULL || pulse_hash == NULL)
		ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE),
						errmsg("pulse_core must be loaded via shared_preload_libraries")));

	InitMaterializedSRF(fcinfo, 0);
	rsinfo = (ReturnSetInfo *)fcinfo->resultinfo;
	LWLockAcquire(pulse_shared->lock, LW_SHARED);
	hash_seq_init(&status, pulse_hash);
	while ((entry = hash_seq_search(&status)) != NULL) {
		Datum values[8];
		bool nulls[8] = {false};
		int64 calls;
		double total_time_ms;
		double min_time_ms;
		double max_time_ms;
		int64 rows;
		TimestampTz last_seen;

		SpinLockAcquire(&entry->mutex);
		calls = entry->calls;
		total_time_ms = entry->total_time_ms;
		min_time_ms = entry->min_time_ms;
		max_time_ms = entry->max_time_ms;
		rows = entry->rows;
		last_seen = entry->last_seen;
		SpinLockRelease(&entry->mutex);

		values[0] = Int64GetDatum((int64)entry->queryid);
		values[1] = Int64GetDatum(calls);
		values[2] = Float8GetDatum(total_time_ms);
		values[3] = Float8GetDatum(calls > 0 ? total_time_ms / calls : 0.0);
		values[4] = Float8GetDatum(min_time_ms);
		values[5] = Float8GetDatum(max_time_ms);
		values[6] = Int64GetDatum(rows);
		values[7] = TimestampTzGetDatum(last_seen);
		tuplestore_putvalues(rsinfo->setResult, rsinfo->setDesc, values, nulls);
	}
	LWLockRelease(pulse_shared->lock);

	return (Datum)0;
}

Datum pulse_reset(PG_FUNCTION_ARGS)
{
	pulse_reset_stats();
	PG_RETURN_VOID();
}
