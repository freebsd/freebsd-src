# Makefile for uuchk
# $FreeBSD: src/gnu/libexec/uucp/uuchk/Makefile,v 1.7.2.1 1999/08/29 14:30:23 peter Exp $

BINDIR=		$(sbindir)

PROG=		uuchk
MAN8=		uuchk.8
SRCS=		uuchk.c
LDADD+=		$(LIBUNIX) $(LIBUUCONF) $(LIBUUCP)
DPADD+=		$(LIBUNIX) $(LIBUUCONF) $(LIBUUCP)
CFLAGS+=	-I$(.CURDIR)/../common_sources\
		-DVERSION=\"$(VERSION)\"

.include <bsd.prog.mk>
