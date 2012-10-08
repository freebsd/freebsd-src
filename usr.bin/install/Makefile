# $FreeBSD$

.include <bsd.own.mk>

PROG=   ninstall
SRCS=   xinstall.c getid.c
MAN=

.PATH: ${.CURDIR}/../../contrib/install
.PATH: ${.CURDIR}/../../contrib/mtree
.PATH: ${.CURDIR}/../../usr.sbin/nmtree
CFLAGS+=	-I${.CURDIR}/../../contrib/mtree \
		-I${.CURDIR}/../../lib/libnetbsd \
		-DNO_SHA384

LIBNETBSDDIR=	${.OBJDIR}/../../lib/libnetbsd
LIBNETBSD=	${LIBNETBSDDIR}/libnetbsd.a
DPADD+=		${LIBNETBSD}
LDADD+=		-lmd ${LIBNETBSD}

.include <bsd.prog.mk>
