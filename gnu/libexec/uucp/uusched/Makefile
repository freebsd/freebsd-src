# Makefile for uusched
# $Id: Makefile,v 1.2 1993/08/06 23:26:18 rgrimes Exp $

BINDIR=		$(bindir)

PROG=	uusched
SRCS=
NOMAN=
STRIP=

uusched:	uusched.in
	sed -e "s|@BINDIR@|$(bindir)|g" -e "s|@SBINDIR@|$(sbindir)|g" \
		$(.CURDIR)/uusched.in > $(.TARGET)

depend:
	@echo -n

.include <bsd.prog.mk>
