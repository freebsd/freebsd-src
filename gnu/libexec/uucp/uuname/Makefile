# Makefile for uuname
# $FreeBSD: src/gnu/libexec/uucp/uuname/Makefile,v 1.5 1999/08/27 23:33:58 peter Exp $

BINDIR=		$(bindir)
BINOWN=		$(owner)
BINMODE=	4555

PROG=		uuname
SRCS=		uuname.c log.c
LDADD+=		$(LIBUNIX) $(LIBUUCONF) $(LIBUUCP)
DPADD+=		$(LIBUNIX) $(LIBUUCONF) $(LIBUUCP)
CFLAGS+=	-I$(.CURDIR)/../common_sources\
		-DVERSION=\"$(VERSION)\"


.include <bsd.prog.mk>
.PATH: $(.CURDIR)/../common_sources
