#	@(#)Makefile	8.1 (Berkeley) 6/4/93
# $FreeBSD: src/libexec/talkd/Makefile,v 1.6.2.2 2001/10/18 12:30:42 des Exp $

PROG=	ntalkd
CFLAGS+=-I${.CURDIR}/../../usr.bin/wall
SRCS=	talkd.c announce.c process.c table.c print.c ttymsg.c
.PATH:  ${.CURDIR}/../../usr.bin/wall
MAN=	talkd.8

.include <bsd.prog.mk>
