# Makefile for uuchk
# $Id: Makefile,v 1.2 1993/08/05 16:15:02 jtc Exp $

BINDIR=		$(sbindir)
BINOWN=		$(owner)

PROG=		uuchk
SRCS=		uuchk.c
LDADD+=		$(LIBUNIX) $(LIBUUCONF) $(LIBUUCP)
DPADD+=		$(LIBUNIX) $(LIBUUCONF) $(LIBUUCP)
CFLAGS+=	-I$(.CURDIR)/../common_sources\
		-DVERSION=\"$(VERSION)\"

NOMAN=		noman

.include <bsd.prog.mk>
