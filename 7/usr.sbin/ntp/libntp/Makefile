# $FreeBSD$

.PATH: ${.CURDIR}/../../../contrib/ntp/libntp \
	${.CURDIR}/../../../contrib/ntp/libisc

LIB= ntp
INTERNALLIB=

NTP_SRCS=	a_md5encrypt.c	adjtime.c	atoint.c	atolfp.c \
	atouint.c	audio.c		authkeys.c \
	authreadkeys.c	authusekey.c	\
	buftvtots.c	caljulian.c	caltontp.c \
	calyearstart.c	clocktime.c	clocktypes.c	decodenetnum.c \
	dofptoa.c	dolfptoa.c	emalloc.c	findconfig.c \
	fptoa.c		fptoms.c	getopt.c \
	hextoint.c	hextolfp.c	humandate.c	icom.c \
	inttoa.c	iosignal.c	lib_strbuf.c \
	machines.c	md5c.c		memmove.c	\
	mfptoa.c	mfptoms.c	mktime.c	modetoa.c \
	mstolfp.c 	ntp_random.c \
	msutotsf.c	msyslog.c	netof.c		ntp_rfc2553.c \
	numtoa.c	numtohost.c	octtoint.c	prettydate.c \
	recvbuff.c	refnumtoa.c	snprintf.c	socktoa.c \
	socktohost.c	strstr.c	systime.c	statestr.c \
	strerror.c	syssignal.c	tsftomsu.c	tstotv.c \
	tvtoa.c	tvtots.c	uglydate.c	uinttoa.c \
	utvtoa.c	ymd2yd.c 

ISC_SRCS= assertions.c error.c inet_ntop.c inet_pton.c interfaceiter.c \
	isc_strerror.c lib.c mem.c msgcat.c net.c netscope.c netaddr.c \
	sockaddr.c

SRCS=	${NTP_SRCS} ${ISC_SRCS}

CFLAGS+= -I${.CURDIR}/../../../contrib/ntp/include -I${.CURDIR}/../

.include <bsd.lib.mk>
