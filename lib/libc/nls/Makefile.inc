#	from $NetBSD: Makefile.inc,v 1.7 1995/02/27 13:06:20 cgd Exp $
#	$Id$

.PATH: ${.CURDIR}/../libc/nls

SRCS+=	catclose.c catgets.c catopen.c msgcat.c

# Only build man pages with libc.
.if ${LIB} == "c"
MAN3+=  nls/catclose.3 nls/catgets.3 nls/catopen.3
.endif
