#	$Id: Makefile,v 1.10 1999/02/27 02:16:01 brian Exp $

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
