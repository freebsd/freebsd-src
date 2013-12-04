NULL=
AUTOMAKE_OPTIONS=

BUILT_SOURCES=
CLEANFILES=

if NTP_BINSUBDIR_IS_BIN
bin_PROGRAMS=	ntpdate
else
sbin_PROGRAMS=	ntpdate
endif

EXTRA_PROGRAMS=	ntptimeset

ntptimeset_SOURCES=	ntptimeset.c ntptime_config.c

AM_CPPFLAGS= -I$(top_srcdir)/include -I$(top_srcdir)/lib/isc/include \
	-I$(top_srcdir)/lib/isc/nothreads/include \
	-I$(top_srcdir)/lib/isc/unix/include

# LDADD might need RESLIB and ADJLIB
LDADD=		version.o ../libntp/libntp.a
ntpdate_LDADD= $(LDADD) @LCRYPTO@
DISTCLEANFILES=	.version version.c stamp-v
noinst_HEADERS=	ntpdate.h
ETAGS_ARGS=	Makefile.am

$(PROGRAMS): $(LDADD)

../libntp/libntp.a:
	cd ../libntp && $(MAKE)

$(top_srcdir)/version :
	cd $(top_srcdir) && $(MAKE) version

version.o: $(ntpdate_OBJECTS) ../libntp/libntp.a Makefile $(top_srcdir)/version
	env CSET=`cat $(top_srcdir)/version` $(top_builddir)/scripts/mkver ntpdate
	$(COMPILE) -c version.c

include $(top_srcdir)/bincheck.mf
include $(top_srcdir)/depsver.mf
