#	from $NetBSD: Makefile.inc,v 1.7 1995/02/27 13:06:20 cgd Exp $
# $FreeBSD: src/lib/libc/nls/Makefile.inc,v 1.8 2002/11/18 09:50:55 ru Exp $

.PATH: ${.CURDIR}/nls

SRCS+=	msgcat.c

MAN+=	catclose.3 catgets.3 catopen.3
