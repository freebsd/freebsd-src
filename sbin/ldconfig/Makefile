#	$Id: Makefile,v 1.3 1993/11/09 04:19:20 paul Exp $

PROG=	ldconfig
SRCS=	ldconfig.c shlib.c etc.c
LDDIR?= $(.CURDIR)/..
LDFLAGS += -static
CFLAGS += -I$(LDDIR) -I$(.CURDIR) -I$(LDDIR)/$(MACHINE) -O
BINDIR= ${DESTDIR}/sbin
MAN8 = ldconfig.8

.PATH: $(LDDIR) $(LDDIR)/$(MACHINE)

.include <bsd.prog.mk>
