#       from: @(#)Makefile    6.2 (Berkeley) 3/25/91
#	$Id: Makefile,v 1.10 1994/02/22 23:34:30 rgrimes Exp $

PROG=           gcc
SRCS=           gcc.c version.c obstack.c
MAN1=		gcc.1 g++.1
MLINKS+=	gcc.1 cc.1
BINDIR=		/usr/bin
CFLAGS+=        -I$(.CURDIR) -I$(.CURDIR)/../lib
.PATH:          $(.CURDIR)/../lib 

afterinstall:
	install -c -o $(BINOWN) -g $(BINGRP) -m $(BINMODE) \
		$(.CURDIR)/g++.script $(DESTDIR)/usr/bin/g++
	install -c -o $(BINOWN) -g $(BINGRP) -m $(BINMODE) \
		$(.CURDIR)/f77.script $(DESTDIR)/usr/bin/f77
	rm -f $(DESTDIR)$(BINDIR)/cc
	cd $(DESTDIR)$(BINDIR); ln -s gcc cc

.include <bsd.prog.mk>
