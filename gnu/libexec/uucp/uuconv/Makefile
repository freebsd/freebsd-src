# Makefile for uuconv
# $Id: Makefile,v 1.4 1995/05/14 17:01:07 ache Exp $

BINDIR=		$(sbindir)

PROG=		uuconv
SRCS=		uuconv.c
LDADD+=		$(LIBUNIX) $(LIBUUCONF) $(LIBUUCP)
DPADD+=		$(LIBUNIX) $(LIBUUCONF) $(LIBUUCP)
CFLAGS+=	-I$(.CURDIR)/../common_sources\
		-I$(.CURDIR)/../libuuconf\
		-DVERSION=\"$(VERSION)\"

NOMAN=		noman

.include <bsd.prog.mk>
