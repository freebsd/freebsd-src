# Makefile for uuname
# $Id: Makefile,v 1.1.10.1 1998/03/03 06:12:56 jkh Exp $

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
