#	@(#)Makefile	8.1 (Berkeley) 5/31/93
# $FreeBSD: src/games/morse/Makefile,v 1.4.6.5 2001/04/25 09:28:58 ru Exp $

PROG=	morse
MAN=	morse.6

.if ${MACHINE_ARCH} == "i386"
CFLAGS += -DSPEAKER=\"/dev/speaker\"
.endif

.include <bsd.prog.mk>
