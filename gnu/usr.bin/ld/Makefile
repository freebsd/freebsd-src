# $Id: Makefile,v 1.4 1993/11/09 04:18:50 paul Exp $
#

PROG=	ld
SRCS=	ld.c symbol.c lib.c shlib.c warnings.c etc.c rrs.c xbits.c md.c
CFLAGS += -static -I$(.CURDIR) -I$(.CURDIR)/$(MACHINE)

LDADD+=	-lgnumalloc
DPADD+= /usr/lib/libgnumalloc.a

SUBDIR=	ldconfig ldd rtld

.PATH: $(.CURDIR)/$(MACHINE)

.include <bsd.prog.mk>
