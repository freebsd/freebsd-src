#	$FreeBSD$
#	$NetBSD: Makefile,v 1.7 2000/05/23 00:46:53 thorpej Exp $
PROG=	raidctl 
SRCS=	rf_configure.c raidctl.c
MAN8=	raidctl.8

LOOKHERE = ${.CURDIR}/../../sys

CFLAGS+= -DRF_UTILITY=1 -I${LOOKHERE}

DPADD=  ${LIBUTIL}
LDADD=  -lutil

.include <bsd.prog.mk>
