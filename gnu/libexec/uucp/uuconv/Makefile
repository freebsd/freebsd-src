# Makefile for uuconv
# $FreeBSD: src/gnu/libexec/uucp/uuconv/Makefile,v 1.8.2.1 1999/08/29 14:30:35 peter Exp $

BINDIR=		$(sbindir)

PROG=		uuconv
SRCS=		uuconv.c
LDADD+=		$(LIBUNIX) $(LIBUUCONF) $(LIBUUCP)
DPADD+=		$(LIBUNIX) $(LIBUUCONF) $(LIBUUCP)
CFLAGS+=	-I$(.CURDIR)/../common_sources\
		-I$(.CURDIR)/../libuuconf\
		-DVERSION=\"$(VERSION)\"

.include <bsd.prog.mk>
