#	$Id: Makefile.inc,v 1.4 1998/09/11 18:49:24 wollman Exp $

CWARNFLAGS=	-Wall -Wnested-externs -Wmissing-prototypes -Wno-unused -Wredundant-decls -Wstrict-prototypes

.if exists(${.OBJDIR}/../common_source)
LIBLPR=	${.OBJDIR}/../common_source/liblpr.a
.else
LIBLPR=	${.CURDIR}/../common_source/liblpr.a
.endif

.include "../Makefile.inc"
