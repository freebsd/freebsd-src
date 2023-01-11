#	Makefile.inc,v 1.2 1994/09/13 21:26:01 wollman Exp
# $FreeBSD$

.PATH:	${LIBC_SRCTOP}/stdtime ${SRCTOP}/contrib/tzcode

TZCODE_SRCS=	asctime.c difftime.c localtime.c
STDTIME_SRCS=	strftime.c strptime.c timelocal.c
SRCS+=		${TZCODE_SRCS} ${STDTIME_SRCS} time32.c

SYM_MAPS+=	${LIBC_SRCTOP}/stdtime/Symbol.map

.for src in ${TZCODE_SRCS} ${STDTIME_SRCS}
CFLAGS.${src}+=	-I${SRCTOP}/contrib/tzcode -include tzconfig.h
.endfor
.for src in ${STDTIME_SRCS}
CFLAGS.${src}+=	-I${LIBC_SRCTOP}/stdtime
.endfor

CFLAGS.localtime.c+= -DALL_STATE -DTHREAD_SAFE
.if ${MK_DETECT_TZ_CHANGES} != "no"
CFLAGS.localtime.c+= -DDETECT_TZ_CHANGES
.endif

MAN+=	ctime.3 strftime.3 strptime.3 time2posix.3
MAN+=	tzfile.5

MLINKS+=ctime.3 asctime.3 ctime.3 difftime.3 ctime.3 gmtime.3 \
	ctime.3 localtime.3 ctime.3 mktime.3 ctime.3 timegm.3 \
	ctime.3 ctime_r.3 ctime.3 localtime_r.3 ctime.3 gmtime_r.3 \
	ctime.3 asctime_r.3
MLINKS+=strftime.3 strftime_l.3
MLINKS+=strptime.3 strptime_l.3
MLINKS+=time2posix.3 posix2time.3
