# Makefile for uustat
# $FreeBSD: src/gnu/libexec/uucp/uustat/Makefile,v 1.5.2.2 2002/07/17 13:25:20 ru Exp $

PROG=		uustat
SRCS=		uustat.c util.c log.c copy.c
LDADD+=		$(LIBUNIX) $(LIBUUCONF) $(LIBUUCP)
DPADD+=		$(LIBUNIX) $(LIBUUCONF) $(LIBUUCP)
CFLAGS+=	-I$(.CURDIR)/../common_sources\
		-DOWNER=\"$(owner)\"\
		-DVERSION=\"$(VERSION)\"

BINDIR=		$(bindir)
BINOWN=		$(owner)
BINGRP=         dialer
BINMODE=        6555

INSTALLFLAGS=	-fschg

.include <bsd.prog.mk>

.PATH: $(.CURDIR)/../common_sources
