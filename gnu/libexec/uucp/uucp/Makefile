# Makefile for uucp
# $FreeBSD: src/gnu/libexec/uucp/uucp/Makefile,v 1.6 1999/08/27 23:33:55 peter Exp $

BINDIR=		$(bindir)
BINOWN=		$(owner)
BINMODE=        4555

PROG=		uucp
SRCS=		uucp.c util.c log.c copy.c
LDADD+=		$(LIBUNIX) $(LIBUUCONF) $(LIBUUCP)
DPADD+=		$(LIBUNIX) $(LIBUUCONF) $(LIBUUCP)
CFLAGS+=	-I$(.CURDIR)/../common_sources\
		-DVERSION=\"$(VERSION)\"

.include <bsd.prog.mk>
.PATH: $(.CURDIR)/../common_sources
