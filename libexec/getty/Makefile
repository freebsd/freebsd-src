#	from: @(#)Makefile	8.1 (Berkeley) 6/4/93
# $FreeBSD$

.include <src.opts.mk>

.if ${MK_CHERI_USE_LLD} == "no"
# broken
WANT_CHERI=none
.endif

PROG=	getty
SRCS=	main.c init.c subr.c chat.c
LIBADD=	util
MAN=	gettytab.5 ttys.5 getty.8

WARNS?=	1
WFORMAT=0

.include <bsd.prog.mk>

