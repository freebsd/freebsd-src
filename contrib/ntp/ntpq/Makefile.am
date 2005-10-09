#AUTOMAKE_OPTIONS = ../util/ansi2knr no-dependencies
AUTOMAKE_OPTIONS = ../util/ansi2knr
bin_PROGRAMS = ntpq
INCLUDES = -I$(top_srcdir)/include
# LDADD might need RESLIB and ADJLIB
ntpq_LDADD = version.o ../libntp/libntp.a @READLINE_LIBS@
DISTCLEANFILES = .version version.c
noinst_HEADERS = ntpq.h
#EXTRA_DIST = ntpq.mak
ETAGS_ARGS = Makefile.am

ntpq_SOURCES = ntpq.c ntpq_ops.c

$(PROGRAMS): $(LDADD)

../libntp/libntp.a:
	cd ../libntp && $(MAKE)

version.o: $(ntpq_OBJECTS) ../libntp/libntp.a Makefile $(top_srcdir)/version
	env CSET=`cat $(top_srcdir)/version` $(top_builddir)/scripts/mkver ntpq
	$(COMPILE) -c version.c
