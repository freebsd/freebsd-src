#	@(#)Makefile	8.1 (Berkeley) 6/5/93
# $FreeBSD: src/sbin/ping/Makefile,v 1.12.2.4 2001/12/19 04:49:11 dd Exp $

PROG=	ping
MAN=	ping.8
BINMODE=4555
CFLAGS+=-Wall -Wmissing-prototypes
.if ${MACHINE_ARCH} == "alpha"
CFLAGS+=-fno-builtin	# GCC's builtin memcpy doesn't do unaligned copies
.endif
DPADD=	${LIBM}
LDADD=	-lm

.if !defined(RELEASE_CRUNCH)
CFLAGS+=-DIPSEC
DPADD+=	${LIBIPSEC}
LDADD+=	-lipsec
.endif

.include <bsd.prog.mk>
