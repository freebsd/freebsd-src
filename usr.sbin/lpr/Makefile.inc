# $FreeBSD: src/usr.sbin/lpr/Makefile.inc,v 1.5.2.1 2001/02/18 16:15:44 ume Exp $

CFLAGS+=-DINET6
CWARNFLAGS=	-Wall -Wnested-externs -Wmissing-prototypes -Wno-unused -Wredundant-decls -Wstrict-prototypes

.if exists(${.OBJDIR}/../common_source)
LIBLPR=	${.OBJDIR}/../common_source/liblpr.a
.else
LIBLPR=	${.CURDIR}/../common_source/liblpr.a
.endif

.include "../Makefile.inc"
