#	$Id: Makefile.inc,v 1.4 1998/03/07 08:54:48 bde Exp $

.if exists(${.OBJDIR}/../lib)
LIBINSTALL=	${.OBJDIR}/../lib/libinstall.a
.else
LIBINSTALL=	${.CURDIR}/../lib/libinstall.a
.endif

# Inherit BINDIR from one level up.
.include "../Makefile.inc"
