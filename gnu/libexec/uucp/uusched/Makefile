# Makefile for uusched
# $Id: Makefile,v 1.4 1995/01/24 17:32:09 jkh Exp $

BINDIR=		$(bindir)

PROG=	uusched
SRCS=
NOMAN=	yes
NOOBJ=	yes
STRIP=

uusched:	uusched.in
	sed -e "s|@BINDIR@|$(bindir)|g" -e "s|@SBINDIR@|$(sbindir)|g" \
		$(.CURDIR)/uusched.in > $(.TARGET)

depend:
	@echo -n

.include <bsd.prog.mk>
