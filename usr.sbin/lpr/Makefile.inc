#	$Id: Makefile.inc,v 1.2 1997/12/17 19:38:30 wollman Exp $

CWARNFLAGS=	-Werror -Wall -Wnested-externs -Wmissing-prototypes -Wno-unused

.if exists(${.OBJDIR}/../common_source)
LIBLPR=	${.OBJDIR}/../common_source/liblpr.a
.else
LIBLPR=	${.CURDIR}/../common_source/liblpr.a
.endif

.include "../Makefile.inc"
