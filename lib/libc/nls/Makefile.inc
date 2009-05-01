#	from $NetBSD: Makefile.inc,v 1.7 1995/02/27 13:06:20 cgd Exp $
# $FreeBSD: src/lib/libc/nls/Makefile.inc,v 1.11.8.1 2009/04/15 03:14:26 kensmith Exp $

.PATH: ${.CURDIR}/nls

SRCS+=	msgcat.c

SYM_MAPS+=${.CURDIR}/nls/Symbol.map

MAN+=	catclose.3 catgets.3 catopen.3
