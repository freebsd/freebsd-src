# Makefile for uulog
# $FreeBSD: src/gnu/libexec/uucp/uulog/Makefile,v 1.4.2.1 1999/08/29 14:30:40 peter Exp $

BINDIR=		$(bindir)

PROG=		uulog
SRCS=		uulog.c log.c
LDADD+=		$(LIBUNIX) $(LIBUUCONF) $(LIBUUCP)
DPADD+=		$(LIBUNIX) $(LIBUUCONF) $(LIBUUCP)
CFLAGS+=	-I$(.CURDIR)/../common_sources\
		-DVERSION=\"$(VERSION)\"

.include <bsd.prog.mk>
.PATH: $(.CURDIR)/../common_sources
