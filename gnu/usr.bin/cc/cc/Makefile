#
# $FreeBSD$
#

PROG =	cc
SRCS =	gcc.c
BINDIR=	/usr/bin
LDDESTDIR+=	-L${.CURDIR}/../cc_int/obj
LDDESTDIR+=	-L${.CURDIR}/../cc_int
LDADD+=	-lcc_int

.include <bsd.prog.mk>
