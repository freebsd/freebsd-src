# $FreeBSD$

.include <bsd.own.mk>

PROG=   ninstall
SRCS=   xinstall.c getid.c util.c
MAN=

.PATH: ${.CURDIR}/../../contrib/install
.PATH: ${.CURDIR}/../../contrib/mtree
.PATH: ${.CURDIR}/../../usr.sbin/nmtree
CFLAGS+=	-I${.CURDIR}/../../contrib/mtree \
		-I${.CURDIR}/../../usr.sbin/nmtree \
		-DNO_SHA384
LDADD+=	-lmd

.include <bsd.prog.mk>
