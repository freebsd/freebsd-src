#AUTOMAKE_OPTIONS = ../ansi2knr no-dependencies
AUTOMAKE_OPTIONS = ansi2knr
bin_PROGRAMS = @MAKE_NTPTIME@ @MAKE_TICKADJ@ @MAKE_TIMETRIM@ ntp-genkeys
EXTRA_PROGRAMS = byteorder hist jitter kern longsize ntptime \
precision sht testrs6000 tickadj timetrim

ntp_genkeys_LDADD = ../ntpd/ntp_config$U.o ../libntp/libntp.a $(LIBRSAREF)
ntp_genkeys_DEPENDENCIES = ../ntpd/ntp_config$U.o ../libntp/libntp.a $(LIBRSAREF)

INCLUDES = -I$(top_srcdir)/include -I$(top_srcdir)/librsaref
# LDADD might need RESLIB and ADJLIB
LDADD =	../libntp/libntp.a
#EXTRA_DIST = README TAGS
EXTRA_DIST = 
ETAGS_ARGS = Makefile.am

../libntp/libntp.a:
	cd ../libntp && $(MAKE)

kern.o: kern.c
	$(COMPILE) -DHAVE_TIMEX_H -c kern.c
