# $FreeBSD$

PROG=sunlabel
SRCS=sunlabel.c geom_sunlabel_enc.c
NOMAN=
WARNS=5

DPADD=	${LIBGEOM} ${LIBBSDXML} ${LIBSBUF}
LDADD=	-lgeom -lbsdxml -lsbuf

.PATH:	${.CURDIR}/../../sys/geom

.include <bsd.prog.mk>

test:	${PROG}
	sh ${.CURDIR}/runtest.sh
