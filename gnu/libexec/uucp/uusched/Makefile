# Makefile for uusched
# $FreeBSD: src/gnu/libexec/uucp/uusched/Makefile,v 1.12.2.2 2001/04/25 14:04:06 ru Exp $

SCRIPTS=	uusched
MAN=		uusched.8
CLEANFILES+=	$(SCRIPTS)

BINDIR=		$(bindir)

uusched: uusched.in
	sed -e "s|@SBINDIR@|$(libxdir)|g" \
		$(.ALLSRC) > $(.TARGET)

.include <bsd.prog.mk>
