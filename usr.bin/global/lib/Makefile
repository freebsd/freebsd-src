# $FreeBSD: src/usr.bin/global/lib/Makefile,v 1.4 1999/08/28 01:01:51 peter Exp $

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
