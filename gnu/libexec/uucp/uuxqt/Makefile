# Makefile for uuxqt
# $FreeBSD$

BINDIR=         $(libxdir)
BINOWN=		$(owner)
BINGRP=         uucp
BINMODE=        6550

PROG=		uuxqt
SRCS=		uuxqt.c util.c log.c copy.c
LDADD+=		$(LIBUNIX) $(LIBUUCONF) $(LIBUUCP)
DPADD+=		$(LIBUNIX) $(LIBUUCONF) $(LIBUUCP)
CFLAGS+=	-I$(.CURDIR)/../common_sources\
		-DVERSION=\"$(VERSION)\" -DOWNER=\"$(owner)\"

MAN=		uuxqt.8

.include <bsd.prog.mk>
.PATH: $(.CURDIR)/../common_sources
