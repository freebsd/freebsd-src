#	@(#)Makefile	8.2 (Berkeley) 3/17/94
# $FreeBSD$

PROG=	bsdlabel
SRCS=	bsdlabel.c geom_bsd_enc.c
#MAN+=	bsdlabel.5
MAN+=	bsdlabel.8

DDADD=  ${LIBGEOM}
LDADD=  -lgeom -lbsdxml -lsbuf

.if ${MACHINE} == "pc98"
CFLAGS+= -DPC98
.endif

.PATH:  ${.CURDIR}/../../sys/geom

.include <bsd.prog.mk>

test:	${PROG}
	sh ${.CURDIR}/runtest.sh

testx:	${PROG}
	sh -x ${.CURDIR}/runtest.sh
