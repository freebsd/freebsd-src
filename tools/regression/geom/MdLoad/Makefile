# $FreeBSD: src/tools/regression/geom/MdLoad/Makefile,v 1.5.20.1 2009/04/15 03:14:26 kensmith Exp $

PROG=	MdLoad
DPADD=	${LIBSBUF} ${LIBBSDXML}
LDADD=	-lsbuf -lbsdxml

NO_MAN=


WARNS?=	4
.include "bsd.prog.mk"

test:	${PROG}
	./${PROG} md34 ../Data/disk.critter.ad0.xml
