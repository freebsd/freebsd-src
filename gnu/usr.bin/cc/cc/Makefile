#
# $FreeBSD$
#

PROG =	cc
SRCS =	gcc.c
BINDIR=	/usr/bin
MLINKS+=cc.1 gcc.1
MLINKS+=cc.1 c++.1
MLINKS+=cc.1 g++.1
LDDESTDIR+=	-L${.CURDIR}/../cc_int/obj
LDDESTDIR+=	-L${.CURDIR}/../cc_int
LDADD+=	-lcc_int

afterinstall:
	cd $(DESTDIR)$(BINDIR) ; rm -f gcc ; ln -s cc gcc

.include <bsd.prog.mk>
