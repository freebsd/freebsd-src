# $FreeBSD$

.PATH:	${.CURDIR}/../../../contrib/pf/libevent


LIB=	event
INTERNALLIB=yes
SRCS=	buffer.c evbuffer.c event.c kqueue.c log.c poll.c select.c signal.c
HDRS=	event.h

CFLAGS+= -I${.CURDIR} \
	-DHAVE_CLOCK_GETTIME \
	-DHAVE_FCNTL_H \
	-DHAVE_POLL \
	-DHAVE_SELECT \
	-DHAVE_SETFD \
	-DHAVE_STDARG_H \
	-DHAVE_SYS_IOCTL_H \
	-DHAVE_SYS_TIME_H \
	-DHAVE_UNISTD_H \
	-DHAVE_VASPRINTF \
	-DHAVE_WORKING_KQUEUE \
	-DVERSION='"1.3b"'

.include <bsd.lib.mk>
