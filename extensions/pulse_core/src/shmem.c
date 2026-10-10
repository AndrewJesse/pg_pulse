#include "postgres.h"
#include "miscadmin.h"
#include "storage/ipc.h"
#include "storage/lwlock.h"
#include "storage/shmem.h"
#include "utils/memutils.h"
#include "utils/timestamp.h"

#include "shmem.h"

HTAB *pulse_hash = NULL;
PulseSharedState *pulse_shared = NULL;

static shmem_request_hook_type prev_shmem_request_hook = NULL;
static shmem_startup_hook_type prev_shmem_startup_hook = NULL;

Size pulse_shmem_size(void)
{
	return MAXALIGN(sizeof(PulseSharedState)) +
		   hash_estimate_size(pulse_max_entries, sizeof(PulseEntry));
}

static void pulse_shmem_request(void)
{
	if (prev_shmem_request_hook)
		prev_shmem_request_hook();

	RequestAddinShmemSpace(pulse_shmem_size());
	RequestNamedLWLockTranche("pulse_core", 1);
}

static void pulse_shmem_startup(void)
{
	bool found;
	HASHCTL info;

	if (prev_shmem_startup_hook)
		prev_shmem_startup_hook();

	LWLockAcquire(AddinShmemInitLock, LW_EXCLUSIVE);
	pulse_shared = ShmemInitStruct("pulse_core", sizeof(PulseSharedState), &found);
	if (!found)
		pulse_shared->lock = &GetNamedLWLockTranche("pulse_core")->lock;

	MemSet(&info, 0, sizeof(info));
	info.keysize = sizeof(uint64);
	info.entrysize = sizeof(PulseEntry);
	pulse_hash = ShmemInitHash("pulse_core hash", pulse_max_entries, pulse_max_entries, &info,
							   HASH_ELEM | HASH_BLOBS);
	LWLockRelease(AddinShmemInitLock);
}

void pulse_install_shmem_hooks(void)
{
	prev_shmem_request_hook = shmem_request_hook;
	shmem_request_hook = pulse_shmem_request;
	prev_shmem_startup_hook = shmem_startup_hook;
	shmem_startup_hook = pulse_shmem_startup;
}

static void pulse_update_entry(PulseEntry *entry, double elapsed_ms, int64 rows)
{
	TimestampTz now = GetCurrentTimestamp();

	SpinLockAcquire(&entry->mutex);
	if (entry->calls == 0)
		entry->min_time_ms = elapsed_ms;
	else if (elapsed_ms < entry->min_time_ms)
		entry->min_time_ms = elapsed_ms;
	if (entry->calls == 0 || elapsed_ms > entry->max_time_ms)
		entry->max_time_ms = elapsed_ms;
	entry->calls++;
	entry->total_time_ms += elapsed_ms;
	entry->rows += rows;
	entry->last_seen = now;
	SpinLockRelease(&entry->mutex);
}

static void pulse_evict_least_called(void)
{
	HASH_SEQ_STATUS status;
	const PulseEntry *entry;
	uint64 victim_queryid = 0;
	int64 lowest_calls = PG_INT64_MAX;
	bool have_victim = false;

	hash_seq_init(&status, pulse_hash);
	while ((entry = hash_seq_search(&status)) != NULL) {
		if (!have_victim || entry->calls < lowest_calls) {
			victim_queryid = entry->queryid;
			lowest_calls = entry->calls;
			have_victim = true;
		}
	}

	if (have_victim)
		hash_search(pulse_hash, &victim_queryid, HASH_REMOVE, NULL);
}

void pulse_record_query(uint64 queryid, double elapsed_ms, int64 rows)
{
	PulseEntry *entry;
	bool found;

	if (pulse_shared == NULL || pulse_hash == NULL)
		return;

	LWLockAcquire(pulse_shared->lock, LW_SHARED);
	entry = hash_search(pulse_hash, &queryid, HASH_FIND, NULL);
	if (entry != NULL) {
		pulse_update_entry(entry, elapsed_ms, rows);
		LWLockRelease(pulse_shared->lock);
		return;
	}
	LWLockRelease(pulse_shared->lock);

	LWLockAcquire(pulse_shared->lock, LW_EXCLUSIVE);
	entry = hash_search(pulse_hash, &queryid, HASH_FIND, NULL);
	if (entry == NULL) {
		if (hash_get_num_entries(pulse_hash) >= pulse_max_entries)
			pulse_evict_least_called();

		entry = hash_search(pulse_hash, &queryid, HASH_ENTER, &found);
		if (!found) {
			MemSet(entry, 0, sizeof(PulseEntry));
			entry->queryid = queryid;
			SpinLockInit(&entry->mutex);
		}
	}
	pulse_update_entry(entry, elapsed_ms, rows);
	LWLockRelease(pulse_shared->lock);
}

void pulse_reset_stats(void)
{
	HASH_SEQ_STATUS status;
	PulseEntry *entry;

	if (pulse_shared == NULL || pulse_hash == NULL)
		ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE),
						errmsg("pulse_core must be loaded via shared_preload_libraries")));

	LWLockAcquire(pulse_shared->lock, LW_EXCLUSIVE);
	hash_seq_init(&status, pulse_hash);
	while ((entry = hash_seq_search(&status)) != NULL)
		hash_search(pulse_hash, &entry->queryid, HASH_REMOVE, NULL);
	LWLockRelease(pulse_shared->lock);
}
