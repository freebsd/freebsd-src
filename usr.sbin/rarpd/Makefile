# @(#) $Header: Makefile,v 1.12 93/07/19 20:07:30 leres Exp $ (LBL)

PROG=	rarpd
CFLAGS+=-g
.if !defined(sunos3) && !defined(sunos4)
SRCS=	rarpd.c etherent.c
BINDIR=/usr/local/sbin
CFLAGS+=-Wall
.endif
.ifdef sunos3
CFLAGS+=-W -DSUNOS3
.endif
.ifdef sunos4
CFLAGS+=-W -DSUNOS4
.endif
MAN8=	rarpd.0

.include <bsd.prog.mk>
