# $FreeBSD: src/usr.bin/global/lib/Makefile,v 1.3.2.1 1999/08/29 15:28:33 peter Exp $

GLOBAL_DIR=	${.CURDIR}/../../../contrib/global
.PATH:		${GLOBAL_DIR}/lib

LIB=		gloutil
SRCS=		gtagsop.c tab.c strbuf.c mgets.c defined.c getdbpath.c \
		find.c dbop.c test.c makepath.c locatestring.c pathop.c \
		conf.c strmake.c usable.c token.c
NOPROFILE=		noprofile
INTERNALLIB=		true
INTERNALSTATICLIB=	true

.include <bsd.lib.mk>
