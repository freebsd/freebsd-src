
INCLUDES = -I$(srcdir)/../include \
	   -I$(includedir)/infiniband

lib_LTLIBRARIES = libosmeventplugin.la

if DEBUG
DBGFLAGS = -ggdb -D_DEBUG_
else
DBGFLAGS = -g
endif

libosmeventplugin_la_CFLAGS = -Wall $(DBGFLAGS) -D_XOPEN_SOURCE=600 -D_BSD_SOURCE=1

if HAVE_LD_VERSION_SCRIPT
    libosmeventplugin_version_script = -Wl,--version-script=$(srcdir)/libosmeventplugin.map
else
    libosmeventplugin_version_script =
endif

osmeventplugin_api_version=$(shell grep LIBVERSION= $(srcdir)/libosmeventplugin.ver | sed 's/LIBVERSION=//')

libosmeventplugin_la_SOURCES = src/osmeventplugin.c
libosmeventplugin_la_LDFLAGS = -version-info $(osmeventplugin_api_version) \
	 -export-dynamic $(libosmeventplugin_version_script)
libosmeventplugin_la_LIBADD = -L../complib $(OSMV_LDADD) -losmcomp
libosmeventplugin_la_DEPENDENCIES = $(srcdir)/libosmeventplugin.map

libosmeventpluginincludedir = $(includedir)/infiniband/complib

libosmeventplugininclude_HEADERS =

# headers are distributed as part of the include dir
EXTRA_DIST = $(srcdir)/libosmeventplugin.map $(srcdir)/libosmeventplugin.ver
