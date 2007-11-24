#AUTOMAKE_OPTIONS = ../util/ansi2knr no-dependencies
AUTOMAKE_OPTIONS = ../util/ansi2knr
#bin_PROGRAMS = ntpdate ntptimeset
bin_PROGRAMS = ntpdate
EXTRA_PROGRAMS = ntptimeset
ntptimeset_SOURCES = ntptimeset.c ntptime_config.c
INCLUDES = -I$(top_srcdir)/include
# LDADD might need RESLIB and ADJLIB
LDADD =	version.o ../libntp/libntp.a
DISTCLEANFILES = .version version.c stamp-v
noinst_HEADERS = ntpdate.h
#EXTRA_DIST = ntpdate.mak
ETAGS_ARGS = Makefile.am

$(PROGRAMS): $(LDADD)

../libntp/libntp.a:
	cd ../libntp && $(MAKE)

version.o: $(ntpdate_OBJECTS) ../libntp/libntp.a Makefile $(top_srcdir)/version
	env CSET=`cat $(top_srcdir)/version` $(top_builddir)/scripts/mkver ntpdate
	$(COMPILE) -c version.c
