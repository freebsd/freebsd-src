#	@(#)Makefile	8.1 (Berkeley) 6/5/93

.include <src.opts.mk>

PACKAGE=runtime
PROG=	ping
SRCS=	main.c
MAN=	ping.8
BINOWN=	root
BINMODE=4555
LIBADD=	m

.if ${MK_INET_SUPPORT}!= "no"
CFLAGS+= -DINET
SRCS+=	ping.c utils.c
.endif

.if ${MK_INET6_SUPPORT} != "no"
CFLAGS+= -DINET6 -DKAME_SCOPEID
SRCS+=	ping6.c
LIBADD+= md
LINKS=	${BINDIR}/ping ${BINDIR}/ping6
.endif

.if ${MK_DYNAMICROOT} == "no"
.warning ${PROG} built without libcasper support
.elif ${MK_CASPER} != "no" && !defined(RESCUE)
LIBADD+=	casper
LIBADD+=	cap_dns
CFLAGS+=-DWITH_CASPER
.endif

CFLAGS+=-DIPSEC
LIBADD+=	ipsec

CFLAGS+=	-Wno-error=unused-but-set-variable

HAS_TESTS=
SUBDIR.${MK_TESTS}+= tests

.include <bsd.prog.mk>
