# $FreeBSD: src/gnu/usr.bin/cvs/lib/Makefile,v 1.19.2.1 2002/07/19 18:46:23 ru Exp $

MAINTAINER=	peter@FreeBSD.org

.include "${.CURDIR}/../Makefile.inc"

.PATH: ${CVSDIR}/src
.PATH: ${CVSDIR}/lib
.PATH: ${CVSDIR}/man

LIB=		cvs
INTERNALLIB=	YES

CFLAGS+= -I${.CURDIR} -I${CVSDIR}/src -DHAVE_CONFIG_H
YFLAGS=

SRCS=	argmatch.c getdate.y getline.c \
	getopt.c getopt1.c savecwd.c \
	sighandle.c stripslash.c \
	xgetwd.c yesno.c

.include <bsd.lib.mk>
