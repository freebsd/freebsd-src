#
# $Id: Makefile.inc,v 1.1 1996/10/20 09:27:46 markm Exp $
#

SHLIB_MAJOR=	3
SHLIB_MINOR=	0

.if exists(${.OBJDIR})
CFLAGS+=	-I${.OBJDIR}
.else
CFLAGS+=	-I${.CURDIR}
.endif
