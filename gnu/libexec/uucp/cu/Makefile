# Makefile for cu
# $FreeBSD: src/gnu/libexec/uucp/cu/Makefile,v 1.7.2.1 1999/08/29 14:29:20 peter Exp $

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

.include <bsd.prog.mk>
.PATH: $(.CURDIR)/../common_sources
