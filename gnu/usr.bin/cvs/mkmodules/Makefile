# $FreeBSD$

PROG = mkmodules
SRCS = mkmodules.c
CFLAGS+= -I${.CURDIR}/../cvs -I${.CURDIR}/../lib -DHAVE_CONFIG_H
DPADD+=	${LIBCVS}
LDADD+=	-lcvs

.include "../../Makefile.inc"
.include <bsd.prog.mk>
