# Makefile for cu
# $Id: Makefile,v 1.2 1994/05/31 05:23:39 ache Exp $

BINDIR=		$(bindir)
BINOWN=		$(owner)
BINMODE=        4555

PROG=		cu
SRCS=		cu.c prot.c log.c chat.c conn.c tcp.c tli.c copy.c
LDADD+=		$(LIBUNIX) $(LIBUUCONF) $(LIBUUCP)
DPADD+=		$(LIBUNIX) $(LIBUUCONF) $(LIBUUCP)
CFLAGS+=	-I$(.CURDIR)/../common_sources\
		-DVERSION=\"$(VERSION)\"

.include <bsd.prog.mk>
.PATH: $(.CURDIR)/../common_sources
