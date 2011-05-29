
SUBDIRS = .

INCLUDES = -I$(srcdir)/include/infiniband

lib_LTLIBRARIES = libibcommon.la

libibcommon_la_CFLAGS = -Wall

if HAVE_LD_VERSION_SCRIPT
libibcommon_version_script = -Wl,--version-script=$(srcdir)/src/libibcommon.map
else
libibcommon_version_script =
endif

libibcommon_la_SOURCES = src/stack.c src/sysfs.c src/util.c src/time.c src/hash.c
libibcommon_la_LDFLAGS = -version-info $(ibcommon_api_version) \
    -export-dynamic $(libibcommon_version_script)
libibcommon_la_DEPENDENCIES = $(srcdir)/src/libibcommon.map

libibcommonincludedir = $(includedir)/infiniband

libibcommoninclude_HEADERS = $(srcdir)/include/infiniband/common.h

EXTRA_DIST = $(srcdir)/include/infiniband/common.h \
	libibcommon.spec.in libibcommon.spec \
	$(srcdir)/src/libibcommon.map libibcommon.ver autogen.sh

dist-hook:
	if [ -x $(top_srcdir)/../gen_chlog.sh ] ; then \
		$(top_srcdir)/../gen_chlog.sh $(PACKAGE) > $(distdir)/ChangeLog ; \
	fi
