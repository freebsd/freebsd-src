#	$Id: Makefile,v 1.8.2.1 1997/08/08 02:18:06 jdp Exp $

PROG=	ldconfig
SRCS=	ldconfig.c shlib.c support.c
LDDIR?= $(.CURDIR)/..
CFLAGS+=-I$(LDDIR) -I$(.CURDIR) -I$(LDDIR)/$(MACHINE)
NOSHARED?= yes
BINDIR=	/sbin
MAN8=	ldconfig.8

.PATH: $(LDDIR) $(LDDIR)/$(MACHINE)

.include <bsd.prog.mk>
