# Makefile for uupick
# $FreeBSD: src/gnu/libexec/uucp/uupick/Makefile,v 1.4.2.1 1999/08/29 14:30:45 peter Exp $

BINDIR=		$(bindir)

PROG=		uupick
SRCS=		uupick.c log.c copy.c
LDADD+=		$(LIBUNIX) $(LIBUUCONF) $(LIBUUCP)
DPADD+=		$(LIBUNIX) $(LIBUUCONF) $(LIBUUCP)
CFLAGS+=	-I$(.CURDIR)/../common_sources\
		-DVERSION=\"$(VERSION)\"

.include <bsd.prog.mk>
.PATH: $(.CURDIR)/../common_sources
