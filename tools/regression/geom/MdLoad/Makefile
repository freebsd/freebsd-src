# $FreeBSD: src/tools/regression/geom/MdLoad/Makefile,v 1.5.16.1 2008/10/02 02:57:24 kensmith Exp $

PROG=	MdLoad
DPADD=	${LIBSBUF} ${LIBBSDXML}
LDADD=	-lsbuf -lbsdxml

NO_MAN=


WARNS?=	4
.include "bsd.prog.mk"

test:	${PROG}
	./${PROG} md34 ../Data/disk.critter.ad0.xml
