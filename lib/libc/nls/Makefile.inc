#	from $NetBSD: Makefile.inc,v 1.7 1995/02/27 13:06:20 cgd Exp $
#	$Id: Makefile.inc,v 1.3 1997/05/03 03:50:01 jb Exp $

.PATH: ${.CURDIR}/../libc/nls

SRCS+=	catclose.c catgets.c catopen.c msgcat.c

.if ${LIB} == "c"
MAN3+=  catclose.3 catgets.3 catopen.3
.endif
