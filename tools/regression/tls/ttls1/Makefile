# $FreeBSD: src/tools/regression/tls/ttls1/Makefile,v 1.2 2004/08/16 09:35:49 dfr Exp $

PROG=	ttls1
LDADD+=	-L../libxx -lxx -Wl,--rpath=${.OBJDIR}/../libxx
LDADD+=	-L../libyy -lyy -Wl,--rpath=${.OBJDIR}/../libyy
NOMAN=	t
DEBUG_FLAGS= -g

.include <bsd.prog.mk>
