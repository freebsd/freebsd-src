#	$Id: Makefile,v 1.2 1998/05/13 06:49:19 jb Exp $

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
CFLAGS+=	-Wall -Wnested-externs -Wmissing-prototypes \
		-Wstrict-prototypes -Wredundant-decls

install:

.include <bsd.lib.mk>
