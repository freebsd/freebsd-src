# Makefile for uuxqt
# $Id: Makefile,v 1.9 1999/03/13 17:35:18 torstenb Exp $

BINDIR=         $(libxdir)
BINOWN=		$(owner)
BINGRP=         $(group)
BINMODE=        6550

PROG=		uuxqt
SRCS=		uuxqt.c util.c log.c copy.c
LDADD+=		$(LIBUNIX) $(LIBUUCONF) $(LIBUUCP)
DPADD+=		$(LIBUNIX) $(LIBUUCONF) $(LIBUUCP)
CFLAGS+=	-I$(.CURDIR)/../common_sources\
		-DVERSION=\"$(VERSION)\" -DOWNER=\"$(owner)\"

MAN8=		uuxqt.8

.include <bsd.prog.mk>
.PATH: $(.CURDIR)/../common_sources
