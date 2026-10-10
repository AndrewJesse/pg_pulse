#ifndef PULSE_CORE_SHMEM_H
#define PULSE_CORE_SHMEM_H

#include "postgres.h"
#include "datatype/timestamp.h"
#include "storage/lwlock.h"
#include "storage/spin.h"
#include "utils/hsearch.h"

typedef struct PulseEntry
{
	uint64 queryid;
	int64 calls;
	double total_time_ms;
	double min_time_ms;
	double max_time_ms;
	int64 rows;
	TimestampTz last_seen;
	slock_t mutex;
} PulseEntry;

typedef struct PulseSharedState
{
	LWLock *lock;
} PulseSharedState;

extern HTAB *pulse_hash;
extern int pulse_max_entries;
extern PulseSharedState *pulse_shared;

Size pulse_shmem_size(void);
void pulse_install_shmem_hooks(void);
void pulse_record_query(uint64 queryid, double elapsed_ms, int64 rows);
void pulse_reset_stats(void);

#endif
