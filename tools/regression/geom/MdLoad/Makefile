# $FreeBSD: src/tools/regression/geom/MdLoad/Makefile,v 1.3 2004/02/24 20:52:58 johan Exp $

PROG=	MdLoad
DPADD=	${LIBSBUF} ${LIBBSDXML}
LDADD=	-lsbuf -lbsdxml

NOMAN=	sorry


WARNS?=	4
.include "bsd.prog.mk"

test:	${PROG}
	./${PROG} md34 ../Data/disk.critter.ad0.xml
