#	$NetBSD: Makefile.inc,v 1.7 1995/02/27 13:06:20 cgd Exp $

.PATH: ${.CURDIR}/nls

SRCS+=	catclose.c catgets.c catopen.c msgcat.c
MAN+=	catclose.3 catgets.3 catopen.3
