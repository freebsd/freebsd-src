# Makefile for uuconv
# $FreeBSD$

BINDIR=		$(sbindir)

PROG=		uuconv
SRCS=		uuconv.c
LDADD+=		$(LIBUNIX) $(LIBUUCONF) $(LIBUUCP)
DPADD+=		$(LIBUNIX) $(LIBUUCONF) $(LIBUUCP)
CFLAGS+=	-I$(.CURDIR)/../common_sources\
		-I$(.CURDIR)/../libuuconf\
		-DVERSION=\"$(VERSION)\"

.include <bsd.prog.mk>
