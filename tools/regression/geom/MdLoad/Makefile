# $FreeBSD: src/tools/regression/geom/MdLoad/Makefile,v 1.5.22.1.4.1 2010/06/14 02:09:06 kensmith Exp $

PROG=	MdLoad
DPADD=	${LIBSBUF} ${LIBBSDXML}
LDADD=	-lsbuf -lbsdxml

NO_MAN=


WARNS?=	4
.include "bsd.prog.mk"

test:	${PROG}
	./${PROG} md34 ../Data/disk.critter.ad0.xml
