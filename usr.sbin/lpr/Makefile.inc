# $FreeBSD: src/usr.sbin/lpr/Makefile.inc,v 1.4.2.1 1999/08/29 15:43:01 peter Exp $

CWARNFLAGS=	-Wall -Wnested-externs -Wmissing-prototypes -Wno-unused -Wredundant-decls -Wstrict-prototypes

.if exists(${.OBJDIR}/../common_source)
LIBLPR=	${.OBJDIR}/../common_source/liblpr.a
.else
LIBLPR=	${.CURDIR}/../common_source/liblpr.a
.endif

.include "../Makefile.inc"
