# Makefile for cu
# $Id: Makefile,v 1.1 1993/08/05 18:23:27 conklin Exp $

BINDIR=		$(bindir)
BINOWN=		$(owner)
BINMODE=	4555

PROG=		cu
SRCS=		cu.c prot.c log.c chat.c conn.c tcp.c tli.c copy.c
LDADD+=		$(LIBUNIX) $(LIBUUCONF) $(LIBUUCP)
DPADD+=		$(LIBUNIX) $(LIBUUCONF) $(LIBUUCP)
CFLAGS+=	-I$(.CURDIR)/../common_sources\
		-DVERSION=\"$(VERSION)\"

.include <bsd.prog.mk>
.PATH: $(.CURDIR)/../common_sources
