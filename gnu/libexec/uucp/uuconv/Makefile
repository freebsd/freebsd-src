# Makefile for uuconv
# $Id: Makefile,v 1.5 1995/05/30 04:45:39 rgrimes Exp $

BINDIR=		$(sbindir)

PROG=		uuconv
SRCS=		uuconv.c
LDADD+=		$(LIBUNIX) $(LIBUUCONF) $(LIBUUCP)
DPADD+=		$(LIBUNIX) $(LIBUUCONF) $(LIBUUCP)
CFLAGS+=	-I$(.CURDIR)/../common_sources\
		-I$(.CURDIR)/../libuuconf\
		-DVERSION=\"$(VERSION)\"

.include <bsd.prog.mk>
