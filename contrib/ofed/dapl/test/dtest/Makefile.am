bin_PROGRAMS = dtest dtestcm
dtest_SOURCES = dtest.c
dtest_CFLAGS = -g -Wall -D_GNU_SOURCE 

dtestcm_SOURCES = dtestcm.c
dtestcm_CFLAGS = -g -Wall -D_GNU_SOURCE 

if EXT_TYPE_IB
bin_PROGRAMS += dtestx
dtestx_SOURCES = dtestx.c
dtestx_CFLAGS = -g -Wall -D_GNU_SOURCE -DDAT_EXTENSIONS
dtestx_LDADD = $(top_builddir)/dat/udat/libdat2.la
endif

INCLUDES =  -I $(srcdir)/../../dat/include 
dtest_LDADD = $(top_builddir)/dat/udat/libdat2.la
dtestcm_LDADD = $(top_builddir)/dat/udat/libdat2.la

