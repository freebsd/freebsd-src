#
# $FreeBSD: src/gnu/lib/libgmp/Makefile.inc,v 1.4 1999/08/27 23:32:50 peter Exp $
#

SHLIB_MAJOR=	3
SHLIB_MINOR=	0

.if exists(${.OBJDIR})
CFLAGS+=	-I${.OBJDIR}
.else
CFLAGS+=	-I${.CURDIR}
.endif
