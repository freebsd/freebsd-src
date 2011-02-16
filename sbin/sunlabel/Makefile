# $FreeBSD: src/sbin/sunlabel/Makefile,v 1.12.34.1 2010/12/21 17:10:29 kensmith Exp $

.PATH: ${.CURDIR}/../../sys/geom

PROG=	sunlabel
SRCS=	sunlabel.c geom_sunlabel_enc.c
MAN=	sunlabel.8

.if ${MACHINE_ARCH} == "sparc64"
LINKS=	${BINDIR}/sunlabel ${BINDIR}/disklabel
MLINKS=	sunlabel.8 disklabel.8
.endif

WARNS?=	5

DPADD=	${LIBGEOM}
LDADD=	-lgeom

.include <bsd.prog.mk>

test:	${PROG}
	sh ${.CURDIR}/runtest.sh

testx: ${PROG}
	sh -x ${.CURDIR}/runtest.sh
