#
# $FreeBSD$
#

PROG =	cpp
SRCS =	cccp.c cexp.c
BINDIR=	/usr/libexec
LDDESTDIR+=	-L${.CURDIR}/../cc_int/obj
LDDESTDIR+=	-L${.CURDIR}/../cc_int
LDADD+=	-lcc_int

.include <bsd.prog.mk>
