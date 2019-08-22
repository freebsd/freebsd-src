# $FreeBSD$

.include <src.opts.mk>

PACKAGE=runtime
PROG=	ping6
MAN=	ping6.8

CFLAGS+=-DIPSEC -DKAME_SCOPEID

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

HAS_TESTS=
SUBDIR.${MK_TESTS}+= tests

.include <bsd.prog.mk>
