INCLUDES = -I$(srcdir)/include

lib_LTLIBRARIES = src/libibcm.la

AM_CFLAGS = -g -Wall -D_GNU_SOURCE

src_libibcm_la_CFLAGS = $(AM_CFLAGS)

if HAVE_LD_VERSION_SCRIPT
    ibcm_version_script = -Wl,--version-script=$(srcdir)/src/libibcm.map
else
    ibcm_version_script =
endif

src_libibcm_la_SOURCES = src/cm.c
src_libibcm_la_LDFLAGS = -version-info 1 -export-dynamic \
			 $(libibcm_version_script)
src_libibcm_la_DEPENDENCIES = $(srcdir)/src/libibcm.map

# Sample program requires use if librdmacm.
#bin_PROGRAMS = examples/ucmpost
#examples_ucmpost_SOURCES = examples/cmpost.c
#examples_ucmpost_LDADD = $(top_builddir)/src/libibcm.la 

libibcmincludedir = $(includedir)/infiniband

libibcminclude_HEADERS = include/infiniband/cm_abi.h \
			 include/infiniband/cm.h

EXTRA_DIST = include/infiniband/cm_abi.h include/infiniband/cm.h \
	     src/libibcm.map libibcm.spec.in

dist-hook: libibcm.spec
	cp libibcm.spec $(distdir)
