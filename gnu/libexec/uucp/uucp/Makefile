# Makefile for uucp
# $FreeBSD: src/gnu/libexec/uucp/uucp/Makefile,v 1.6.2.2 2002/07/17 13:25:19 ru Exp $

PROG=		uucp
SRCS=		uucp.c util.c log.c copy.c
LDADD+=		$(LIBUNIX) $(LIBUUCONF) $(LIBUUCP)
DPADD+=		$(LIBUNIX) $(LIBUUCONF) $(LIBUUCP)
CFLAGS+=	-I$(.CURDIR)/../common_sources\
		-DVERSION=\"$(VERSION)\"

BINDIR=		$(bindir)
BINOWN=		$(owner)
BINMODE=        4555
INSTALLFLAGS=	-fschg

.include <bsd.prog.mk>

.PATH: $(.CURDIR)/../common_sources
