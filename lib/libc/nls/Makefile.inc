#	from $NetBSD: Makefile.inc,v 1.7 1995/02/27 13:06:20 cgd Exp $
# $FreeBSD: src/lib/libc/nls/Makefile.inc,v 1.4.2.1 1999/08/29 14:47:42 peter Exp $

.PATH: ${.CURDIR}/../libc/nls

SRCS+=	catclose.c catgets.c catopen.c msgcat.c

.if ${LIB} == "c"
MAN3+=  catclose.3 catgets.3 catopen.3
.endif
