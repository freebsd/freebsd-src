#
# $Id$
#

SHLIB_MAJOR=	3
SHLIB_MINOR=	0

.if exists(${.OBJDIR})
CFLAGS+=	-I${.OBJDIR}
.else
CFLAGS+=	-I${.CURDIR}
.endif
