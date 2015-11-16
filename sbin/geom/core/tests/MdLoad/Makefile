# $FreeBSD$

BINDIR=	${TESTSBASE}/sbin/geom/core/MdLoad

PROG=	MdLoad

DPADD=	${LIBSBUF} ${LIBBSDXML}
LDADD=	-lsbuf -lbsdxml

MAN=

WARNS?=	4

check: .PHONY
check: ${PROG}
	./${PROG} md34 ../Data/disk.critter.ad0.xml

.include <bsd.prog.mk>
