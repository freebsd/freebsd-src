# Makefile for uustat
# $Id: Makefile,v 1.2 1995/09/15 00:05:07 ache Exp $

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

.include <bsd.prog.mk>
.PATH: $(.CURDIR)/../common_sources
