
if DEBUG
DBGFLAGS = -ggdb -D_DEBUG_
else
DBGFLAGS = -g
endif

INCLUDES = -I$(srcdir)/include $(OSMV_INCLUDES)

sbin_PROGRAMS = osmtest
osmtest_SOURCES = main.c osmtest.c osmt_service.c osmt_slvl_vl_arb.c \
		 osmt_multicast.c osmt_inform.c
if OSMV_VAPI
osmtest_SOURCES += osmt_mtl_regular_qp.c
endif
osmtest_CFLAGS = -Wall $(DBGFLAGS)
osmtest_LDADD = -L../complib -losmcomp -L../libvendor -losmvendor -L../opensm -lopensm $(OSMV_LDADD)

EXTRA_DIST = $(srcdir)/include/osmt_inform.h \
   $(srcdir)/include/osmtest_subnet.h \
   $(srcdir)/include/osmtest.h \
   $(srcdir)/include/osmt_mtl_regular_qp.h \
   $(srcdir)/include/osmtest_base.h
