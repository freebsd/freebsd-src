# $FreeBSD: src/tools/regression/tls/ttls1/Makefile,v 1.2.2.1 2005/02/13 07:23:36 obrien Exp $

PROG=	ttls1
LDADD+=	-L../libxx -lxx -Wl,--rpath=${.OBJDIR}/../libxx
LDADD+=	-L../libyy -lyy -Wl,--rpath=${.OBJDIR}/../libyy
NOMAN=
DEBUG_FLAGS= -g

.include <bsd.prog.mk>
