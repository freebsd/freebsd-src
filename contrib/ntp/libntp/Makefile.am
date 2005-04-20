#AUTOMAKE_OPTIONS = ../ansi2knr no-dependencies
AUTOMAKE_OPTIONS = ../util/ansi2knr
noinst_LIBRARIES = libntp.a @MAKE_LIBNTPSIM@
EXTRA_LIBRARIES = libntpsim.a
libntp_a_SRCS = a_md5encrypt.c adjtime.c atoint.c atolfp.c atouint.c \
	audio.c authkeys.c authreadkeys.c authusekey.c binio.c buftvtots.c \
	caljulian.c calleapwhen.c caltontp.c calyearstart.c clocktime.c \
	clocktypes.c decodenetnum.c dofptoa.c dolfptoa.c emalloc.c \
	findconfig.c fptoa.c fptoms.c getopt.c gpstolfp.c hextoint.c \
	hextolfp.c humandate.c icom.c ieee754io.c inttoa.c iosignal.c \
	lib_strbuf.c machines.c md5c.c memmove.c mfp_mul.c mfptoa.c \
	mfptoms.c mktime.c modetoa.c mstolfp.c msutotsf.c msyslog.c netof.c \
	ntp_rfc2553.c numtoa.c numtohost.c octtoint.c prettydate.c \
	ranny.c recvbuff.c refnumtoa.c snprintf.c socktoa.c socktohost.c \
	statestr.c strdup.c strerror.c strstr.c \
	syssignal.c tsftomsu.c tstotv.c tvtoa.c tvtots.c \
	uglydate.c uinttoa.c utvtoa.c ymd2yd.c \
	$(srcdir)/../libisc/assertions.c $(srcdir)/../libisc/error.c \
	$(srcdir)/../libisc/interfaceiter.c $(srcdir)/../libisc/lib.c \
	$(srcdir)/../libisc/mem.c $(srcdir)/../libisc/msgcat.c \
	$(srcdir)/../libisc/net.c $(srcdir)/../libisc/isc_strerror.c \
	$(srcdir)/../libisc/inet_ntop.c
libntp_a_SOURCES = systime.c $(libntp_a_SRCS)
libntpsim_a_SOURCES = systime_s.c $(libntp_a_SRCS)
EXTRA_libntp_a_SOURCES = adjtimex.c log.c random.c
INCLUDES = -I$(top_srcdir)/include -I../include
ETAGS_ARGS = Makefile.am

noinst_HEADERS = lib_strbuf.h log.h

../include/des.h:
	touch ../include/des.h

EXTRA_DIST = README

