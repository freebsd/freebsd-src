# Makefile for cu
# $Id: Makefile,v 1.4 1995/08/19 21:24:49 ache Exp $

BINDIR=		$(bindir)
BINOWN=		$(owner)
BINMODE=        4555

PROG=		cu
SRCS=           cu.c prot.c log.c chat.c conn.c copy.c
LDADD+=		$(LIBUNIX) $(LIBUUCONF) $(LIBUUCP)
DPADD+=		$(LIBUNIX) $(LIBUUCONF) $(LIBUUCP)
CFLAGS+=	-I$(.CURDIR)/../common_sources\
		-DVERSION=\"$(VERSION)\"

.include <bsd.prog.mk>
.PATH: $(.CURDIR)/../common_sources
