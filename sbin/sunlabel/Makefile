# $FreeBSD$

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
