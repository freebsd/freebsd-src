#AUTOMAKE_OPTIONS = ../ansi2knr no-dependencies
AUTOMAKE_OPTIONS = ansi2knr
bin_PROGRAMS = @MAKE_TICKADJ@ @MAKE_NTPTIME@
EXTRA_PROGRAMS = byteorder hist jitter kern longsize ntptime \
precision tickadj testrs6000 timetrim sht

INCLUDES = -I$(top_srcdir)/include
# LDADD might need RESLIB and ADJLIB
LDADD =	../libntp/libntp.a
#EXTRA_DIST = README TAGS
EXTRA_DIST = 
ETAGS_ARGS = Makefile.am

../libntp/libntp.a:
	cd ../libntp && $(MAKE)

kern.o: kern.c
	$(COMPILE) -DHAVE_TIMEX_H -c kern.c
