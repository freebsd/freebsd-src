# $FreeBSD$

.if exists(${.OBJDIR}/../lib)
LIBINSTALL=	${.OBJDIR}/../lib/libinstall.a
.else
LIBINSTALL=	${.CURDIR}/../lib/libinstall.a
.endif

MANSECT?=	1

# Inherit BINDIR from one level up.
.include "../Makefile.inc"
