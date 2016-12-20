EXTENSION  = permuteseq
EXTVERSION = 1.0.0

PG_CONFIG = pg_config

DATA = sql/$(EXTENSION)--$(EXTVERSION).sql
MODULE_big = permuteseq
OBJS      = permuteseq.o

all:

#all: sql/$(EXTENSION)--$(EXTVERSION).sql 

#$(EXTENSION)--$(EXTVERSION).sql: sql/$(EXTENSION).sql
#	cp $< $@

PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
