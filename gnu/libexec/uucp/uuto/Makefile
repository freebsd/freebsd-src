# Makefile for uuto
# $Id: Makefile,v 1.2 1993/08/05 16:14:06 jtc Exp $

BINDIR=		$(bindir)

PROG=		uuto
SRCS=
NOMAN=
STRIP=

uuto:	uuto.in
	sed -e "s|@BINDIR@|$(bindir)|g" -e "s|@SBINDIR@|$(sbindir)|g" \
		$(.CURDIR)/uuto.in > $(.TARGET)

.include <bsd.prog.mk>
