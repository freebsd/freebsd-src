# Makefile for uuto
# $Id: Makefile,v 1.1 1993/08/05 18:28:09 conklin Exp $

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
