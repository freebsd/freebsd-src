# $FreeBSD: src/usr.sbin/lpr/Makefile.inc,v 1.5.2.2 2002/02/01 01:03:39 gad Exp $

CFLAGS+= -DINET6
CWARNFLAGS=	-Wall -Wnested-externs -Wmissing-prototypes -Wno-unused -Wredundant-decls -Wstrict-prototypes

.if exists(${.OBJDIR}/../common_source)
LIBLPR=	${.OBJDIR}/../common_source/liblpr.a
.else
LIBLPR=	${.CURDIR}/../common_source/liblpr.a
.endif

.include "../Makefile.inc"
