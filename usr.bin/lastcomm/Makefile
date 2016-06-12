#	From: @(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD$

.include <src.opts.mk>

PACKAGE=acct

PROG=	lastcomm
SRCS=	lastcomm.c readrec.c

.if ${MK_TESTS} != "no"
SUBDIR+= tests
.endif

.include <bsd.prog.mk>
