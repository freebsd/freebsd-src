# $FreeBSD: src/tools/regression/geom/MdLoad/Makefile,v 1.1 2003/04/09 12:34:20 phk Exp $

PROG=	MdLoad

LDADD=	-lsbuf -lbsdxml

NOMAN=	sorry


WARNS=4
.include "bsd.prog.mk"

test:	${PROG}
	./${PROG} md34 ../Data/disk.critter.ad0.xml
