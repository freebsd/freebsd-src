# $FreeBSD$

PROG=sunlabel
SRCS=sunlabel.c geom_sunlabel_enc.c
MAN=	sunlabel.8
WARNS=5

DPADD=	${LIBGEOM}
LDADD=	-lgeom

.PATH:	${.CURDIR}/../../sys/geom

.include <bsd.prog.mk>

test:	${PROG}
	sh ${.CURDIR}/runtest.sh
