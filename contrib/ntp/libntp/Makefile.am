#AUTOMAKE_OPTIONS = ../ansi2knr no-dependencies
AUTOMAKE_OPTIONS = ../util/ansi2knr
noinst_LIBRARIES = libntp.a
libntp_a_SOURCES = a_md5encrypt.c adjtime.c atoint.c atolfp.c atouint.c \
	audio.c authencrypt.c authkeys.c authparity.c authreadkeys.c \
	authusekey.c binio.c buftvtots.c caljulian.c calleapwhen.c caltontp.c \
	calyearstart.c clocktime.c clocktypes.c decodenetnum.c dofptoa.c \
	dolfptoa.c emalloc.c findconfig.c fptoa.c fptoms.c getopt.c \
	gpstolfp.c hextoint.c hextolfp.c humandate.c icom.c ieee754io.c \
	inttoa.c iosignal.c lib_strbuf.c machines.c md5c.c memmove.c \
	mfp_mul.c mfptoa.c mfptoms.c mktime.c modetoa.c mstolfp.c msutotsf.c \
	msyslog.c netof.c numtoa.c numtohost.c octtoint.c prettydate.c \
	ranny.c recvbuff.c refnumtoa.c snprintf.c statestr.c strdup.c \
	strerror.c syssignal.c systime.c tsftomsu.c tstotv.c tvtoa.c tvtots.c \
	uglydate.c uinttoa.c utvtoa.c ymd2yd.c
EXTRA_libntp_a_SOURCES = adjtimex.c log.c random.c
INCLUDES = -I$(top_srcdir)/include -I$(top_srcdir)/librsaref
ETAGS_ARGS = Makefile.am

noinst_HEADERS = lib_strbuf.h log.h

../include/des.h:
	touch ../include/des.h

EXTRA_DIST = README

#mktime_.c: mktime.c $(ANSI2KNR)
#	$(ANSI2KNR) $< mktime_.c

#strerror_.c: strerror.c $(ANSI2KNR)
#	$(ANSI2KNR) $< strerror_.c
