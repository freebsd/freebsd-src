# Makefile for uuto
# $FreeBSD$

SCRIPTS=	uuto
MAN=		uuto.1
CLEANFILES+=	$(SCRIPTS)

BINDIR=		$(bindir)

uuto: uuto.in
	sed -e "s|@BINDIR@|$(bindir)|g" \
		$(.ALLSRC) > $(.TARGET)

.include <bsd.prog.mk>
