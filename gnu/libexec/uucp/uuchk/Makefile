# Makefile for uuchk
# $Id: Makefile,v 1.6 1997/02/22 15:28:25 peter Exp $

BINDIR=		$(sbindir)

PROG=		uuchk
MAN8=		uuchk.8
SRCS=		uuchk.c
LDADD+=		$(LIBUNIX) $(LIBUUCONF) $(LIBUUCP)
DPADD+=		$(LIBUNIX) $(LIBUUCONF) $(LIBUUCP)
CFLAGS+=	-I$(.CURDIR)/../common_sources\
		-DVERSION=\"$(VERSION)\"

.include <bsd.prog.mk>
