# Makefile for uux
# $FreeBSD$

BINDIR=		$(bindir)
BINOWN=		$(owner)
BINMODE=        4555

PROG=		uux
SRCS=		uux.c util.c log.c copy.c
LDADD+=		$(LIBUNIX) $(LIBUUCONF) $(LIBUUCP)
DPADD+=		$(LIBUNIX) $(LIBUUCONF) $(LIBUUCP)
CFLAGS+=	-I$(.CURDIR)/../common_sources\
		-DVERSION=\"$(VERSION)\"
INSTALLFLAGS+= -fschg

.include <bsd.prog.mk>
.PATH: $(.CURDIR)/../common_sources
