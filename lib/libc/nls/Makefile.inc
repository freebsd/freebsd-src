#	from $NetBSD: Makefile.inc,v 1.7 1995/02/27 13:06:20 cgd Exp $
# $FreeBSD$

.PATH: ${.CURDIR}/../libc/nls

SRCS+=	msgcat.c

.if ${LIB} == "c"
MAN3+=  catclose.3 catgets.3 catopen.3
.endif
