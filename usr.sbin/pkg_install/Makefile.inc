#	$Id: Makefile.inc,v 1.3.8.1 1998/10/28 12:00:31 jkh Exp $

.if exists(${.OBJDIR}/../lib)
LIBINSTALL=	${.OBJDIR}/../lib/libinstall.a
.else
LIBINSTALL=	${.CURDIR}/../lib/libinstall.a
.endif

# Inherit BINDIR from one level up.
.include "../Makefile.inc"
