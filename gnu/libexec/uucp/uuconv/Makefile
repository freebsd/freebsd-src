# Makefile for uuconv
# $Id: Makefile,v 1.3 1994/05/31 05:46:34 ache Exp $

BINDIR=		$(sbindir)
BINOWN=		$(owner)
BINGRP=         $(group)
BINMODE=        550

PROG=		uuconv
SRCS=		uuconv.c 
LDADD+=		$(LIBUNIX) $(LIBUUCONF) $(LIBUUCP)
DPADD+=		$(LIBUNIX) $(LIBUUCONF) $(LIBUUCP)
CFLAGS+=	-I$(.CURDIR)/../common_sources\
		-I$(.CURDIR)/../libuuconf\
		-DVERSION=\"$(VERSION)\"

NOMAN=		noman

.include <bsd.prog.mk>
