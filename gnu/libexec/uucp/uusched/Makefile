# Makefile for uusched
# $Id: Makefile,v 1.1 1993/08/05 18:27:55 conklin Exp $

BINDIR=		$(bindir)

PROG=	uusched
SRCS=
NOMAN=
STRIP=

uusched:	uusched.in
	sed -e "s|@BINDIR@|$(bindir)|g" -e "s|@SBINDIR@|$(sbindir)|g" \
		$(.CURDIR)/uusched.in > $(.TARGET)

depend:
	

.include <bsd.prog.mk>
