#	$Id: Makefile,v 1.10 1997/02/22 15:46:37 peter Exp $

PROG=	ldconfig
SRCS=	ldconfig.c shlib.c support.c
LDDIR?= $(.CURDIR)/..
CFLAGS+=-I$(LDDIR) -I$(.CURDIR) -I$(LDDIR)/$(MACHINE)
NOSHARED?= yes
BINDIR=	/sbin
MAN8=	ldconfig.8

.PATH: $(LDDIR) $(LDDIR)/$(MACHINE)

.include <bsd.prog.mk>
