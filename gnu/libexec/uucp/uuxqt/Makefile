# Makefile for uuxqt
# $Id: Makefile,v 1.5 1994/05/31 05:46:42 ache Exp $

BINDIR=		$(sbindir)
BINOWN=		$(owner)
BINGRP=         $(group)
BINMODE=        4550

PROG=		uuxqt
SRCS=		uuxqt.c util.c log.c copy.c
LDADD+=		$(LIBUNIX) $(LIBUUCONF) $(LIBUUCP)
DPADD+=		$(LIBUNIX) $(LIBUUCONF) $(LIBUUCP)
CFLAGS+=	-I$(.CURDIR)/../common_sources\
		-DVERSION=\"$(VERSION)\" -DOWNER=\"$(owner)\"

MAN8=		uuxqt.8

.include <bsd.prog.mk>
.PATH: $(.CURDIR)/../common_sources
