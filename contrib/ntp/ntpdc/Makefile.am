#AUTOMAKE_OPTIONS = ../util/ansi2knr no-dependencies
AUTOMAKE_OPTIONS = ../util/ansi2knr
bin_PROGRAMS = ntpdc
INCLUDES = -I$(top_srcdir)/include
# LDADD might need RESLIB and ADJLIB
LDADD =	version.o ../libntp/libntp.a @LIBRSAREF@ 
DISTCLEANFILES = .version version.c
noinst_HEADERS = ntpdc.h
#EXTRA_DIST = ntpdc.mak
ETAGS_ARGS = Makefile.am

ntpdc_SOURCES = ntpdc.c ntpdc_ops.c

$(PROGRAMS): $(LDADD)

../libntp/libntp.a:
	cd ../libntp && $(MAKE)

version.o: $(ntpdc_OBJECTS) ../libntp/libntp.a @LIBRSAREF@ Makefile
	$(top_builddir)/scripts/mkver ntpdc
	$(COMPILE) -c version.c
