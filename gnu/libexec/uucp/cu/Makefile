# Makefile for cu
# $FreeBSD: src/gnu/libexec/uucp/cu/Makefile,v 1.8.2.1 2001/09/10 20:22:57 dillon Exp $

BINDIR=		$(bindir)
BINOWN=		$(owner)
BINGRP=         dialer
BINMODE=        6555

PROG=		cu
SRCS=           cu.c prot.c log.c chat.c conn.c copy.c
LDADD+=		$(LIBUNIX) $(LIBUUCONF) $(LIBUUCP)
DPADD+=		$(LIBUNIX) $(LIBUUCONF) $(LIBUUCP)
CFLAGS+=	-I$(.CURDIR)/../common_sources\
		-DVERSION=\"$(VERSION)\"
INSTALLFLAGS+= -fschg

.include <bsd.prog.mk>
.PATH: $(.CURDIR)/../common_sources
