PG_CONFIG ?= pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
EXTENSION = pulse_core
MODULE_big = pulse_core
OBJS = src/pulse_core.o src/shmem.o src/tracker.o src/sqlapi.o
DATA = pulse_core--0.1.0.sql
REGRESS = basic reset gucs nested
REGRESS_OPTS = --temp-instance=./tmp_check --temp-config=test/pulse_core.conf --inputdir=test
PG_CPPFLAGS += -I$(includedir_server)
include $(PGXS)
