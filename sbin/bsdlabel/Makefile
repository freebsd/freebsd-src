#	@(#)Makefile	8.2 (Berkeley) 3/17/94
# $FreeBSD$

.PATH: ${.CURDIR}/../../sys/geom

PROG=	bsdlabel
SRCS=	bsdlabel.c geom_bsd_enc.c
#MAN=	bsdlabel.5
MAN+=	bsdlabel.8

.if ${MACHINE_ARCH} == "i386" || ${MACHINE_ARCH} == "alpha" || \
    ${MACHINE_ARCH} == "amd64"
LINKS=	${BINDIR}/bsdlabel ${BINDIR}/disklabel
MLINKS=	bsdlabel.8 disklabel.8
.endif

DPADD=	${LIBGEOM}
LDADD=	-lgeom

.if ${MACHINE} == "pc98"
CFLAGS+=-DPC98
.endif

.include <bsd.prog.mk>

test: ${PROG}
	sh ${.CURDIR}/runtest.sh

testx: ${PROG}
	sh -x ${.CURDIR}/runtest.sh
