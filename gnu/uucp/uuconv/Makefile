# Makefile for uuconv
# $Id: Makefile,v 1.1 1993/08/05 18:27:28 conklin Exp $

BINDIR=		$(sbindir)
BINOWN=		$(owner)

PROG=		uuconv
SRCS=		uuconv.c 
LDADD+=		$(LIBUNIX) $(LIBUUCONF) $(LIBUUCP)
DPADD+=		$(LIBUNIX) $(LIBUUCONF) $(LIBUUCP)
CFLAGS+=	-I$(.CURDIR)/../common_sources\
		-I$(.CURDIR)/../libuuconf\
		-DVERSION=\"$(VERSION)\"

NOMAN=		noman

.include <bsd.prog.mk>
