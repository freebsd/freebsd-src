# Makefile for uucp
# $FreeBSD$

BINDIR=		$(bindir)
BINOWN=		$(owner)
BINMODE=        4555

PROG=		uucp
SRCS=		uucp.c util.c log.c copy.c
LDADD+=		$(LIBUNIX) $(LIBUUCONF) $(LIBUUCP)
DPADD+=		$(LIBUNIX) $(LIBUUCONF) $(LIBUUCP)
CFLAGS+=	-I$(.CURDIR)/../common_sources\
		-DVERSION=\"$(VERSION)\"

.include <bsd.prog.mk>
.PATH: $(.CURDIR)/../common_sources
