# $FreeBSD: src/gnu/usr.bin/cvs/libdiff/Makefile,v 1.4 2000/01/10 11:38:50 phantom Exp $

MAINTAINER=	peter@FreeBSD.org

.include "${.CURDIR}/../Makefile.inc"

.PATH:	${CVSDIR}/diff

LIB=		diff

NOPROFILE=	yes
NOPIC=		yes

CFLAGS+= -I${.CURDIR} -I${.CURDIR}/../lib -I${CVSDIR}/lib -DHAVE_CONFIG_H

SRCS =	diff.c diff3.c analyze.c cmpbuf.c cmpbuf.h io.c context.c ed.c \
        normal.c ifdef.c util.c dir.c version.c diff.h side.c

install:
	@echo -n

.include <bsd.lib.mk>
