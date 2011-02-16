# $FreeBSD: src/tools/regression/geom/MdLoad/Makefile,v 1.5.26.1 2010/12/21 17:10:29 kensmith Exp $

PROG=	MdLoad
DPADD=	${LIBSBUF} ${LIBBSDXML}
LDADD=	-lsbuf -lbsdxml

NO_MAN=


WARNS?=	4
.include "bsd.prog.mk"

test:	${PROG}
	./${PROG} md34 ../Data/disk.critter.ad0.xml
