# Makefile for cu
# $FreeBSD: src/gnu/libexec/uucp/cu/Makefile,v 1.8.2.2 2002/07/17 13:25:19 ru Exp $

PROG=		cu
SRCS=           cu.c prot.c log.c chat.c conn.c copy.c
DPADD+=		$(LIBUNIX) $(LIBUUCONF) $(LIBUUCP)
LDADD+=		$(LIBUNIX) $(LIBUUCONF) $(LIBUUCP)
CFLAGS+=	-I$(.CURDIR)/../common_sources\
		-DVERSION=\"$(VERSION)\"

BINDIR=		$(bindir)
BINOWN=		$(owner)
BINGRP=         dialer
BINMODE=        6555
INSTALLFLAGS=	-fschg

.include <bsd.prog.mk>

.PATH: $(.CURDIR)/../common_sources
