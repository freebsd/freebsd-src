#	$Id: Makefile.inc,v 1.3 1998/03/07 09:49:00 bde Exp $

CFLAGS+=	-DRSRR

.if exists(${.OBJDIR}/../common)
LIBMROUTED=	${.OBJDIR}/../common/libmrouted.a
.else
LIBMROUTED=	${.CURDIR}/../common/libmrouted.a
.endif

.include "../Makefile.inc"
