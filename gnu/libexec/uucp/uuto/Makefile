# Makefile for uuto
# $FreeBSD: src/gnu/libexec/uucp/uuto/Makefile,v 1.10.2.2 2001/04/25 14:04:07 ru Exp $

SCRIPTS=	uuto
MAN=		uuto.1
CLEANFILES+=	$(SCRIPTS)

BINDIR=		$(bindir)

uuto: uuto.in
	sed -e "s|@BINDIR@|$(bindir)|g" \
		$(.ALLSRC) > $(.TARGET)

.include <bsd.prog.mk>
