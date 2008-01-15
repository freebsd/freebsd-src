#	@(#)Makefile	8.1 (Berkeley) 6/5/93
# $FreeBSD: src/sbin/ping/Makefile,v 1.19 2004/02/23 20:13:55 johan Exp $

PROG=	ping
MAN=	ping.8
BINOWN=	root
BINMODE=4555
.if ${MACHINE_ARCH} == "alpha"
CFLAGS+=-fno-builtin	# GCC's builtin memcpy doesn't do unaligned copies
.endif
WARNS?=	2
DPADD=	${LIBM}
LDADD=	-lm

.if !defined(RELEASE_CRUNCH)
CFLAGS+=-DIPSEC
DPADD+=	${LIBIPSEC}
LDADD+=	-lipsec
.endif

.include <bsd.prog.mk>
