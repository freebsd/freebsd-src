# Makefile for uupick
# $Id: Makefile,v 1.3 1997/02/22 15:28:45 peter Exp $

BINDIR=		$(bindir)

PROG=		uupick
SRCS=		uupick.c log.c copy.c
LDADD+=		$(LIBUNIX) $(LIBUUCONF) $(LIBUUCP)
DPADD+=		$(LIBUNIX) $(LIBUUCONF) $(LIBUUCP)
CFLAGS+=	-I$(.CURDIR)/../common_sources\
		-DVERSION=\"$(VERSION)\"

.include <bsd.prog.mk>
.PATH: $(.CURDIR)/../common_sources
