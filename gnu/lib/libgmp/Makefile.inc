#
# $Id: Makefile.inc,v 1.3 1997/02/22 15:42:57 peter Exp $
#

SHLIB_MAJOR=	3
SHLIB_MINOR=	0

.if exists(${.OBJDIR})
CFLAGS+=	-I${.OBJDIR}
.else
CFLAGS+=	-I${.CURDIR}
.endif
