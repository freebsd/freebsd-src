# $FreeBSD: src/tools/regression/geom/MdLoad/Makefile,v 1.5.18.1 2008/11/25 02:59:29 kensmith Exp $

PROG=	MdLoad
DPADD=	${LIBSBUF} ${LIBBSDXML}
LDADD=	-lsbuf -lbsdxml

NO_MAN=


WARNS?=	4
.include "bsd.prog.mk"

test:	${PROG}
	./${PROG} md34 ../Data/disk.critter.ad0.xml
