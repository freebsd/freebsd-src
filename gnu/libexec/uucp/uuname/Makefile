# Makefile for uuname
# $FreeBSD: src/gnu/libexec/uucp/uuname/Makefile,v 1.5.2.2 2002/07/17 13:25:19 ru Exp $

PROG=		uuname
SRCS=		uuname.c log.c
LDADD+=		$(LIBUNIX) $(LIBUUCONF) $(LIBUUCP)
DPADD+=		$(LIBUNIX) $(LIBUUCONF) $(LIBUUCP)
CFLAGS+=	-I$(.CURDIR)/../common_sources\
		-DVERSION=\"$(VERSION)\"

BINDIR=		$(bindir)
BINOWN=		$(owner)
BINMODE=	4555
INSTALLFLAGS=	-fschg

.include <bsd.prog.mk>

.PATH: $(.CURDIR)/../common_sources
