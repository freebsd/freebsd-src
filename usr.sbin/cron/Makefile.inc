# $FreeBSD$

.if exists(${.OBJDIR}/../lib)
LIBCRON= ${.OBJDIR}/../lib/libcron.a
.else
LIBCRON= ${.CURDIR}/../lib/libcron.a
.endif

.include "../Makefile.inc"
