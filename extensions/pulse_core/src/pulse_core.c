#include "postgres.h"
#include "fmgr.h"
#include "miscadmin.h"
#include "utils/guc.h"

#include "shmem.h"
#include "tracker.h"

PG_MODULE_MAGIC;

int pulse_max_entries = 1000;
int pulse_track = 1;

static const struct config_enum_entry pulse_track_options[] = {
	{"none", 0, false},
	{"top", 1, false},
	{"all", 2, false},
	{NULL, 0, false},
};

void _PG_init(void)
{
	if (!process_shared_preload_libraries_in_progress)
		ereport(ERROR,
				(errmsg("pulse_core must be loaded via shared_preload_libraries"),
				 errhint("Add pulse_core to shared_preload_libraries and restart PostgreSQL.")));

	DefineCustomIntVariable(
		"pulse.max_entries", "Sets the maximum number of query statistics entries.", NULL,
		&pulse_max_entries, 1000, 100, 100000, PGC_POSTMASTER, 0, NULL, NULL, NULL);
	DefineCustomEnumVariable("pulse.track", "Selects which statements are tracked.", NULL,
							 &pulse_track, 1, pulse_track_options, PGC_SUSET, 0, NULL, NULL, NULL);
	MarkGUCPrefixReserved("pulse");

	pulse_install_shmem_hooks();
	pulse_install_executor_hooks();
}
