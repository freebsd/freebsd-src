#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.bin/xinstall/Makefile,v 1.20 2002/11/13 13:49:29 ru Exp $

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
