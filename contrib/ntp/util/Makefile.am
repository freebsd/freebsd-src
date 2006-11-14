#AUTOMAKE_OPTIONS = ../ansi2knr no-dependencies
AUTOMAKE_OPTIONS=	ansi2knr
bin_PROGRAMS=	@MAKE_NTPTIME@ @MAKE_TICKADJ@ @MAKE_TIMETRIM@ @MAKE_NTP_KEYGEN@
EXTRA_PROGRAMS=	audio-pcm byteorder hist jitter kern longsize \
	ntp-keygen ntptime pps-api precision sht testrs6000 tickadj timetrim

INCLUDES=	-I$(top_srcdir)/include
# LDADD might need RESLIB and ADJLIB
LDADD=		../libntp/libntp.a
ntp_keygen_LDADD=	@LCRYPTO@
#EXTRA_DIST=	README TAGS
EXTRA_DIST=
ETAGS_ARGS=	Makefile.am

../libntp/libntp.a:
	cd ../libntp && $(MAKE)

kern.o: kern.c
	$(COMPILE) -DHAVE_TIMEX_H -c kern.c
