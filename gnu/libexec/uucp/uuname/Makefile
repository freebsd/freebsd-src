# Makefile for uuname
# $FreeBSD: src/gnu/libexec/uucp/uuname/Makefile,v 1.5.2.1 2001/09/10 20:22:57 dillon Exp $

BINDIR=		$(bindir)
BINOWN=		$(owner)
BINMODE=	4555

PROG=		uuname
SRCS=		uuname.c log.c
LDADD+=		$(LIBUNIX) $(LIBUUCONF) $(LIBUUCP)
DPADD+=		$(LIBUNIX) $(LIBUUCONF) $(LIBUUCP)
CFLAGS+=	-I$(.CURDIR)/../common_sources\
		-DVERSION=\"$(VERSION)\"
INSTALLFLAGS+= -fschg

.include <bsd.prog.mk>
.PATH: $(.CURDIR)/../common_sources
