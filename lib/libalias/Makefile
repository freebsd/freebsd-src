#	$Id: Makefile,v 1.9.2.1 1999/03/24 17:12:00 brian Exp $

LIB=		alias
SHLIB_MAJOR=	3
SHLIB_MINOR=	0
CFLAGS+=	-Wall -I${.CURDIR}
SRCS=		alias.c alias_cuseeme.c alias_db.c alias_ftp.c alias_irc.c \
		alias_nbt.c alias_proxy.c alias_util.c
MAN3=		libalias.3 

beforeinstall:
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 ${.CURDIR}/alias.h \
	    ${DESTDIR}/usr/include

.include <bsd.lib.mk>
