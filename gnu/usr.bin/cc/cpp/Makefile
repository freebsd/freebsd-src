#	From: @(#)Makefile	6.4 (Berkeley) 2/21/91
#	$Id: Makefile,v 1.5 1993/08/28 10:29:09 rgrimes Exp $

PROG=		gcpp
BINDIR=		/usr/libexec
SRCS=		cccp.c cexp.y version.c
MAN1=		gcpp.1
CFLAGS+=	-I. -I$(.CURDIR) -I$(.CURDIR)/../lib \
		-DGCC_INCLUDE_DIR=\"\" \
		-DGPLUSPLUS_INCLUDE_DIR=\"/usr/include/g++\"
YFLAGS=

MLINKS=	gcpp.1 cpp.1

.PATH:		$(.CURDIR)/../lib

afterinstall:
	install -c -o $(BINOWN) -g $(BINGRP) -m $(BINMODE) \
		$(.CURDIR)/usr.bin.cpp.sh $(DESTDIR)/usr/bin/gcpp
	@/bin/rm -rf $(DESTDIR)/usr/bin/cpp
	@cd $(DESTDIR)/usr/bin; ln -s gcpp cpp
	@echo $(DESTDIR)/usr/bin/cpp -\> $(DESTDIR)/usr/bin/gcpp
	@/bin/rm -rf $(DESTDIR)$(BINDIR)/cpp
	@cd $(DESTDIR)$(BINDIR); ln -s gcpp cpp
	@echo $(DESTDIR)$(BINDIR)/cpp -\> $(DESTDIR)$(BINDIR)/gcpp

.include <bsd.prog.mk>
