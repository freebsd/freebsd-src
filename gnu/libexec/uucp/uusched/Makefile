# Makefile for uusched
# $Id: Makefile,v 1.2 1993/08/05 16:14:03 jtc Exp $

BINDIR=		$(bindir)

PROG=	uusched
SRCS=
NOMAN=
STRIP=

uusched:	uusched.in
	sed -e "s|@BINDIR@|$(bindir)|g" -e "s|@SBINDIR@|$(sbindir)|g" \
		$(.CURDIR)/uusched.in > $(.TARGET)

.include <bsd.prog.mk>
