# Makefile for uuconv
# $Id: Makefile,v 1.5.6.1 1998/03/03 06:10:11 jkh Exp $

BINDIR=		$(sbindir)

PROG=		uuconv
SRCS=		uuconv.c
LDADD+=		$(LIBUNIX) $(LIBUUCONF) $(LIBUUCP)
DPADD+=		$(LIBUNIX) $(LIBUUCONF) $(LIBUUCP)
CFLAGS+=	-I$(.CURDIR)/../common_sources\
		-I$(.CURDIR)/../libuuconf\
		-DVERSION=\"$(VERSION)\"

.include <bsd.prog.mk>
