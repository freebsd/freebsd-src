# Makefile for uusched
# $Id: Makefile,v 1.3 1995/01/23 21:19:45 jkh Exp $

BINDIR=		$(bindir)

PROG=	uusched
SRCS=
NOMAN=	yes
NOOBJS=	yes
STRIP=

uusched:	uusched.in
	sed -e "s|@BINDIR@|$(bindir)|g" -e "s|@SBINDIR@|$(sbindir)|g" \
		$(.CURDIR)/uusched.in > $(.TARGET)

depend:
	@echo -n

.include <bsd.prog.mk>
