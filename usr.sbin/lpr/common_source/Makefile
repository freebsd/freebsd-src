#	$Id: Makefile,v 1.4 1998/09/11 18:49:31 wollman Exp $

#
# Library of internal routines for the print spooler suite.
# Originally these were compiled separately into each program,
# but the library makes it much easier to modularize them.
#
LIB=		lpr
SRCS=		common.c displayq.c net.c printcap.c request.c rmjob.c \
		startdaemon.c
NOMAN=		noman
NOPROFILE=	noprofile
NOPIC=		nopic
CFLAGS+=	${CWARNFLAGS}

install:

.include <bsd.lib.mk>
