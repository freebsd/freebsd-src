# Makefile for uusched
# $FreeBSD$

SCRIPTS=	uusched
MAN=		uusched.8
CLEANFILES+=	$(SCRIPTS)

BINDIR=		$(bindir)

uusched: uusched.in
	sed -e "s|@SBINDIR@|$(libxdir)|g" \
		$(.ALLSRC) > $(.TARGET)

.include <bsd.prog.mk>
