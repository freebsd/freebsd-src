# Makefile for uuchk
# $Id: Makefile,v 1.4.6.1 1998/03/03 06:09:27 jkh Exp $

BINDIR=		$(sbindir)

PROG=		uuchk
MAN8=		uuchk.8
SRCS=		uuchk.c
LDADD+=		$(LIBUNIX) $(LIBUUCONF) $(LIBUUCP)
DPADD+=		$(LIBUNIX) $(LIBUUCONF) $(LIBUUCP)
CFLAGS+=	-I$(.CURDIR)/../common_sources\
		-DVERSION=\"$(VERSION)\"

.include <bsd.prog.mk>
