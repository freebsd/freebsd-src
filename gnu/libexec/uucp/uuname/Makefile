# Makefile for uuname
# $Id: Makefile,v 1.3 1997/02/22 15:28:43 peter Exp $

BINDIR=		$(bindir)
BINOWN=		$(owner)
BINMODE=	4555

PROG=		uuname
SRCS=		uuname.c log.c
LDADD+=		$(LIBUNIX) $(LIBUUCONF) $(LIBUUCP)
DPADD+=		$(LIBUNIX) $(LIBUUCONF) $(LIBUUCP)
CFLAGS+=	-I$(.CURDIR)/../common_sources\
		-DVERSION=\"$(VERSION)\"


.include <bsd.prog.mk>
.PATH: $(.CURDIR)/../common_sources
