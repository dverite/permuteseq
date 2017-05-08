EXTENSION  = permuteseq
EXTVERSION = 1.2.0

PG_CONFIG = pg_config

DATA = $(wildcard sql/*.sql)

MODULE_big = permuteseq
OBJS      = permuteseq.o

all:


PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
