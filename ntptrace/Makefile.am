#AUTOMAKE_OPTIONS = ../util/ansi2knr no-dependencies
AUTOMAKE_OPTIONS = ../util/ansi2knr
bin_PROGRAMS = ntptrace
INCLUDES = -I$(top_srcdir)/include
# LDADD might need RESLIB and ADJLIB
LDADD =	version.o ../libntp/libntp.a
DISTCLEANFILES = .version version.c
noinst_HEADERS = ntptrace.h
#EXTRA_DIST = ntptrace.mak README TAGS save
ETAGS_ARGS = Makefile.am

$(PROGRAMS): $(LDADD)

../libntp/libntp.a:
	cd ../libntp && $(MAKE)

version.o: $(ntptrace_OBJECTS) ../libntp/libntp.a Makefile
	env CSET=`cat $(top_srcdir)/version` $(top_builddir)/scripts/mkver ntptrace
	$(COMPILE) -c version.c
