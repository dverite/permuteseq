EXTENSION  = permuteseq
EXTVERSION = 1.1.0

PG_CONFIG = pg_config

DATA = sql/$(EXTENSION)--$(EXTVERSION).sql
MODULE_big = permuteseq
OBJS      = permuteseq.o

all:


PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
