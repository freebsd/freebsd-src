#	$Id: Makefile,v 1.6 1994/02/13 20:42:18 jkh Exp $

PROG=	ldconfig
SRCS=	ldconfig.c shlib.c etc.c
LDDIR?= $(.CURDIR)/..
CFLAGS+=-I$(LDDIR) -I$(.CURDIR) -I$(LDDIR)/$(MACHINE)
LDSTATIC=-static
BINDIR=	/sbin
MAN8=	ldconfig.8

.PATH: $(LDDIR) $(LDDIR)/$(MACHINE)

.include <bsd.prog.mk>
