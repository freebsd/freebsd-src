# $FreeBSD: src/tools/regression/geom/MdLoad/Makefile,v 1.3.2.1 2005/02/13 07:23:33 obrien Exp $

PROG=	MdLoad
DPADD=	${LIBSBUF} ${LIBBSDXML}
LDADD=	-lsbuf -lbsdxml

NOMAN=


WARNS?=	4
.include "bsd.prog.mk"

test:	${PROG}
	./${PROG} md34 ../Data/disk.critter.ad0.xml
