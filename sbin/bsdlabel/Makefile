#	@(#)Makefile	8.2 (Berkeley) 3/17/94
# $FreeBSD$

PROG=	bsdlabel
SRCS=	bsdlabel.c geom_bsd_enc.c
#MAN+=	bsdlabel.5
MAN+=	bsdlabel.8

.if ${MACHINE} == "pc98"
CFLAGS+= -DPC98
.endif

.PATH:  ${.CURDIR}/../../sys/geom

.include <bsd.prog.mk>

test:	${PROG}
	sh ${.CURDIR}/runtest.sh
