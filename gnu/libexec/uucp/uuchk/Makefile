# Makefile for uuchk
# $Id: Makefile,v 1.2 1994/05/31 05:11:12 ache Exp $

BINDIR=		$(sbindir)
BINOWN=		$(owner)
BINGRP=         $(group)
BINMODE=        550

PROG=		uuchk
SRCS=		uuchk.c
LDADD+=		$(LIBUNIX) $(LIBUUCONF) $(LIBUUCP)
DPADD+=		$(LIBUNIX) $(LIBUUCONF) $(LIBUUCP)
CFLAGS+=	-I$(.CURDIR)/../common_sources\
		-DVERSION=\"$(VERSION)\"

NOMAN=		noman

.include <bsd.prog.mk>
