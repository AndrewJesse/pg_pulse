#include "postgres.h"
#include "replication/output_plugin.h"
#include "utils/builtins.h"

PG_MODULE_MAGIC;

void _PG_output_plugin_init(OutputPluginCallbacks *cb)
{
	(void)cb;
	ereport(LOG, (errmsg("pulse_decoder initialized")));
}
