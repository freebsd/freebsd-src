# $FreeBSD: src/gnu/usr.bin/cvs/libdiff/Makefile,v 1.5 2002/05/13 11:24:01 ru Exp $

MAINTAINER=	peter@FreeBSD.org

.include "${.CURDIR}/../Makefile.inc"

.PATH:	${CVSDIR}/diff

LIB=		diff
INTERNALLIB=	YES

CFLAGS+= -I${.CURDIR} -I${.CURDIR}/../lib -I${CVSDIR}/lib -DHAVE_CONFIG_H

SRCS =	diff.c diff3.c analyze.c cmpbuf.c cmpbuf.h io.c context.c ed.c \
        normal.c ifdef.c util.c dir.c version.c diff.h side.c

.include <bsd.lib.mk>
