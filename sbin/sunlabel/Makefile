# $FreeBSD$

PROG=	sunlabel
SRCS=	sunlabel.c sunlabel_enc.c
MAN=	sunlabel.8

.if ${MACHINE_CPUARCH} == "sparc64"
LINKS=	${BINDIR}/sunlabel ${BINDIR}/disklabel
MLINKS=	sunlabel.8 disklabel.8
.endif

LIBADD=	geom

.include <bsd.prog.mk>

test:	${PROG}
	sh ${.CURDIR}/runtest.sh

testx: ${PROG}
	sh -x ${.CURDIR}/runtest.sh
