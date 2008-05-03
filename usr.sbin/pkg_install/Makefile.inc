# $FreeBSD$

.include <bsd.own.mk>

LIBINSTALL=	${.OBJDIR}/../lib/libinstall.a


# Inherit BINDIR from one level up.
.include "../Makefile.inc"
