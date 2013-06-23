#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD$

PROG=		xinstall
PROGNAME=	install
SRCS=		xinstall.c getid.c
MAN=		install.1

.PATH:		${.CURDIR}/../../contrib/mtree
.PATH:		${.CURDIR}/../../lib/libmd
CFLAGS+=	-I${.CURDIR}/../../contrib/mtree
CFLAGS+=	-I${.CURDIR}/../../lib/libnetbsd
CFLAGS+=	-I${.CURDIR}/../../lib/libmd

DPADD+=		${LIBMD}
LDADD+=		-lmd

.include <bsd.prog.mk>
