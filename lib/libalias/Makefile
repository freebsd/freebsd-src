# $FreeBSD: src/lib/libalias/Makefile,v 1.9.2.3 1999/08/29 14:55:16 peter Exp $

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
