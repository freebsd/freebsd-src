#
# $FreeBSD: src/gnu/lib/libgmp/Makefile.inc,v 1.3.2.1 1999/08/29 14:28:50 peter Exp $
#

SHLIB_MAJOR=	3
SHLIB_MINOR=	0

.if exists(${.OBJDIR})
CFLAGS+=	-I${.OBJDIR}
.else
CFLAGS+=	-I${.CURDIR}
.endif
