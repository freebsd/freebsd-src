#	@(#)Makefile	8.1 (Berkeley) 6/5/93
# $FreeBSD$

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
