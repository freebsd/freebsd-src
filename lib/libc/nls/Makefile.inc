#	from $NetBSD: Makefile.inc,v 1.7 1995/02/27 13:06:20 cgd Exp $
# $FreeBSD$

.PATH: ${.CURDIR}/nls

SRCS+=	msgcat.c

MAN+=	catclose.3 catgets.3 catopen.3
