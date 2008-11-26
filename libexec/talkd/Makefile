#	@(#)Makefile	8.1 (Berkeley) 6/4/93
# $FreeBSD: src/libexec/talkd/Makefile,v 1.9.30.1 2008/10/02 02:57:24 kensmith Exp $

PROG=	ntalkd
SRCS=	talkd.c announce.c process.c table.c print.c ttymsg.c
.PATH:  ${.CURDIR}/../../usr.bin/wall
MAN=	talkd.8
CFLAGS+=-I${.CURDIR}/../../usr.bin/wall
WFORMAT=0

.include <bsd.prog.mk>
