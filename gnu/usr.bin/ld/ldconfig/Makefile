#	$Id: Makefile,v 1.11 1997/06/29 21:40:54 bde Exp $

PROG=	ldconfig
SRCS=	ldconfig.c shlib.c support.c
LDDIR?= $(.CURDIR)/..
CFLAGS+=-I$(LDDIR) -I$(.CURDIR) -I$(LDDIR)/$(MACHINE)
NOSHARED?= yes
BINDIR=	/sbin
MAN8=	ldconfig.8

.PATH: $(LDDIR) $(LDDIR)/$(MACHINE)

.include <bsd.prog.mk>
