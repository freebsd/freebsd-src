#	$Id: Makefile,v 1.1 1997/12/02 20:45:17 wollman Exp $

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
.if ${MACHINE_ARCH} == "i386"
CFLAGS+=	-Werror
.endif

install:

.include <bsd.lib.mk>
