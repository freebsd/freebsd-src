# $FreeBSD: src/usr.sbin/lpr/common_source/Makefile,v 1.5.2.2 2002/02/01 01:03:39 gad Exp $

#
# Library of internal routines for the print spooler suite.
# Originally these were compiled separately into each program,
# but the library makes it much easier to modularize them.
#
LIB=		lpr
NOPROFILE=	noprofile
NOPIC=		nopic
NOMAN=		noman
SRCS=		common.c ctlinfo.c displayq.c net.c printcap.c request.c \
		rmjob.c startdaemon.c

CFLAGS+=	${CWARNFLAGS}

install:

.include <bsd.lib.mk>
