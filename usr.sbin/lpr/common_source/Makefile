# $FreeBSD$

#
# Library of internal routines for the print spooler suite.
# Originally these were compiled separately into each program,
# but the library makes it much easier to modularize them.
#
LIB=		lpr
SRCS=		common.c ctlinfo.c displayq.c net.c printcap.c request.c \
		rmjob.c startdaemon.c
NOMAN=		noman
NOPROFILE=	noprofile
NOPIC=		nopic
CFLAGS+=	${CWARNFLAGS}

install:

.include <bsd.lib.mk>
