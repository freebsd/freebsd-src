# Makefile for uuchk
# $FreeBSD: src/gnu/libexec/uucp/uuchk/Makefile,v 1.8.2.1 2001/04/25 12:03:20 ru Exp $

BINDIR=		$(sbindir)

PROG=		uuchk
MAN=		uuchk.8
LDADD+=		$(LIBUNIX) $(LIBUUCONF) $(LIBUUCP)
DPADD+=		$(LIBUNIX) $(LIBUUCONF) $(LIBUUCP)
CFLAGS+=	-I$(.CURDIR)/../common_sources\
		-DVERSION=\"$(VERSION)\"

.include <bsd.prog.mk>
