#	@(#)Makefile	8.1 (Berkeley) 5/31/93
# $FreeBSD: src/games/morse/Makefile,v 1.4.6.3 2001/01/25 21:08:30 phantom Exp $

PROG=	morse
#MAN6=	morse.6
NOMAN=yes

.if ${MACHINE_ARCH} == "i386"
CFLAGS += -DSPEAKER=\"/dev/speaker\"
.endif

.include <bsd.prog.mk>
