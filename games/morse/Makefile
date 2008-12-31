#	@(#)Makefile	8.1 (Berkeley) 5/31/93
# $FreeBSD: src/games/morse/Makefile,v 1.8.6.1 2008/11/25 02:59:29 kensmith Exp $

PROG=	morse
MAN=	morse.6

.if ${MACHINE_ARCH} == "i386" || ${MACHINE_ARCH} == "amd64"
CFLAGS += -DSPEAKER=\"/dev/speaker\"
.endif

.include <bsd.prog.mk>
