# Makefile for uux
# $Id: Makefile,v 1.2 1994/05/31 05:46:38 ache Exp $

BINDIR=		$(bindir)
BINOWN=		$(owner)
BINMODE=        4555

PROG=		uux
SRCS=		uux.c util.c log.c copy.c
LDADD+=		$(LIBUNIX) $(LIBUUCONF) $(LIBUUCP)
DPADD+=		$(LIBUNIX) $(LIBUUCONF) $(LIBUUCP)
CFLAGS+=	-I$(.CURDIR)/../common_sources\
		-DVERSION=\"$(VERSION)\"

.include <bsd.prog.mk>
.PATH: $(.CURDIR)/../common_sources
