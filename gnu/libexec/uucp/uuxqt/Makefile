# Makefile for uuxqt
# $Id: Makefile,v 1.2 1993/08/05 16:15:28 jtc Exp $

BINDIR=		$(sbindir)
BINOWN=		$(owner)
BINMODE=	4555

PROG=		uuxqt
SRCS=		uuxqt.c util.c log.c copy.c
LDADD+=		$(LIBUNIX) $(LIBUUCONF) $(LIBUUCP)
DPADD+=		$(LIBUNIX) $(LIBUUCONF) $(LIBUUCP)
CFLAGS+=	-I$(.CURDIR)/../common_sources\
		-DVERSION=\"$(VERSION)\"

MAN8=		uuxqt.0

.include <bsd.prog.mk>
.PATH: $(.CURDIR)/../common_sources
