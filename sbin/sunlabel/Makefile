# $FreeBSD$

PROG=sunlabel
SRCS=sunlabel.c geom_sunlabel_enc.c
LINKS=${BINDIR}/sunlabel ${BINDIR}/disklabel
NOMAN=
WARNS=5

.PATH:	${.CURDIR}/../../sys/geom

.include <bsd.prog.mk>

test:	${PROG}
	sh ${.CURDIR}/runtest.sh
