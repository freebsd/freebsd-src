#
# $FreeBSD$
#

PROG =	c++
SRCS =	g++.c
BINDIR=	/usr/bin
NOMAN=	1
LDDESTDIR+=	-L${.CURDIR}/../cc_int/obj
LDDESTDIR+=	-L${.CURDIR}/../cc_int
LDADD+=	-lcc_int

.include <bsd.prog.mk>
