# Makefile for uustat
# $FreeBSD: src/gnu/libexec/uucp/uustat/Makefile,v 1.5.2.1 2001/09/10 20:22:57 dillon Exp $

BINDIR=		$(bindir)
BINOWN=		$(owner)
BINGRP=         dialer
BINMODE=        6555

PROG=		uustat
SRCS=		uustat.c util.c log.c copy.c
LDADD+=		$(LIBUNIX) $(LIBUUCONF) $(LIBUUCP)
DPADD+=		$(LIBUNIX) $(LIBUUCONF) $(LIBUUCP)
CFLAGS+=	-I$(.CURDIR)/../common_sources\
		-DOWNER=\"$(owner)\"\
		-DVERSION=\"$(VERSION)\"
INSTALLFLAGS+= -fschg

.include <bsd.prog.mk>
.PATH: $(.CURDIR)/../common_sources
