# Makefile for uuxqt
# $FreeBSD: src/gnu/libexec/uucp/uuxqt/Makefile,v 1.8.2.2 1999/08/29 14:30:56 peter Exp $

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
