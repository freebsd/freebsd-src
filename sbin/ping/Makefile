#	@(#)Makefile	8.1 (Berkeley) 6/5/93
# $FreeBSD: src/sbin/ping/Makefile,v 1.7.2.1 1999/08/29 15:15:06 peter Exp $

PROG=	ping
MAN8=	ping.8
BINMODE=4555
COPTS+=	-Wall -Wmissing-prototypes
.if ${MACHINE_ARCH} == "alpha"
COPTS+= -fno-builtin	# GCC's builtin memcpy doesn't do unaligned copies
.endif
DPADD=	${LIBM}
LDADD=	-lm

.include <bsd.prog.mk>
