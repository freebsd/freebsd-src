#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD$

PROG=		xinstall
PROGNAME=	install
SRCS=		xinstall.c
MAN=		install.1

.if defined(BOOTSTRAPPING) && \
    ( ${BOOTSTRAPPING} < 400021 || \
    ( ${BOOTSTRAPPING} >= 500000 && ${BOOTSTRAPPING} < 500007 ))
.PATH: ${.CURDIR}/../../lib/libc/gen
SRCS+=		strtofflags.c
.endif

.include <bsd.prog.mk>
