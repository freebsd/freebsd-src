# Makefile for uuchk
# $Id: Makefile,v 1.4 1995/05/14 17:00:58 ache Exp $

BINDIR=		$(sbindir)

PROG=		uuchk
MAN8=		uuchk.8
SRCS=		uuchk.c
LDADD+=		$(LIBUNIX) $(LIBUUCONF) $(LIBUUCP)
DPADD+=		$(LIBUNIX) $(LIBUUCONF) $(LIBUUCP)
CFLAGS+=	-I$(.CURDIR)/../common_sources\
		-DVERSION=\"$(VERSION)\"

.include <bsd.prog.mk>
