# $FreeBSD$

.include <src.opts.mk>

PACKAGE=runtime
PROG=	ping6
MAN=	ping6.8

CFLAGS+=-DIPSEC -DKAME_SCOPEID
WARNS?=	3

BINOWN=	root
BINMODE=4555

LIBADD=	ipsec m md

.if ${MK_DYNAMICROOT} == "no"
.warning ${PROG} built without libcasper support
.elif ${MK_CASPER} != "no" && !defined(RESCUE)
LIBADD+=	casper
LIBADD+=	cap_dns
CFLAGS+=-DWITH_CASPER
.endif

.include <bsd.prog.mk>
