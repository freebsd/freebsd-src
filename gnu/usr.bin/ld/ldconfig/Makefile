#	$Id: Makefile,v 1.2 1993/11/03 05:20:49 cgd Exp $

PROG=	ldconfig
SRCS=	ldconfig.c shlib.c etc.c
LDDIR?= $(.CURDIR)/..
LDFLAGS += -static
CFLAGS += -I$(LDDIR) -I$(.CURDIR) -I$(LDDIR)/$(MACHINE) -O
BINDIR= ${DESTDIR}/sbin
MAN8 = ldconfig.0

.PATH: $(LDDIR) $(LDDIR)/$(MACHINE)

.include <bsd.prog.mk>
