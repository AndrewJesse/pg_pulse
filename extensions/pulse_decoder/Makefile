PG_CONFIG ?= pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
MODULE_big = pulse_decoder
OBJS = src/pulse_decoder.o
PG_CPPFLAGS += -I$(includedir_server)
include $(PGXS)
