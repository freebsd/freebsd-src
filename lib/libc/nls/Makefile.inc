#	from $NetBSD: Makefile.inc,v 1.7 1995/02/27 13:06:20 cgd Exp $
# $FreeBSD: src/lib/libc/nls/Makefile.inc,v 1.5.2.2 2001/04/25 10:04:07 ru Exp $

.PATH: ${.CURDIR}/../libc/nls

SRCS+=	msgcat.c

.if ${LIB} == "c"
MAN+=	catclose.3 catgets.3 catopen.3
.endif
