# Makefile for uuto
# $Id: Makefile,v 1.2 1993/08/06 23:28:02 rgrimes Exp $

BINDIR=		$(bindir)

PROG=		uuto
SRCS=
NOMAN=
STRIP=

uuto:	uuto.in
	sed -e "s|@BINDIR@|$(bindir)|g" -e "s|@SBINDIR@|$(sbindir)|g" \
		$(.CURDIR)/uuto.in > $(.TARGET)

depend:

.include <bsd.prog.mk>
