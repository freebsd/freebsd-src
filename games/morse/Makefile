#	@(#)Makefile	8.1 (Berkeley) 5/31/93
# $FreeBSD: src/games/morse/Makefile,v 1.7.14.1 2005/11/16 10:50:10 ru Exp $

PROG=	morse
MAN=	morse.6

.if ${MACHINE_ARCH} == "i386" || ${MACHINE_ARCH} == "amd64"
CFLAGS += -DSPEAKER=\"/dev/speaker\"
.endif

.include <bsd.prog.mk>
