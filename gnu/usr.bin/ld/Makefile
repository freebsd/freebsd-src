# $Id: Makefile,v 1.8 1993/11/03 13:01:36 cgd Exp $
#

PROG=	ld
SRCS=	ld.c symbol.c lib.c shlib.c warnings.c etc.c rrs.c xbits.c md.c
CFLAGS += -g -I$(.CURDIR) -I$(.CURDIR)/$(MACHINE)

LDADD+=	-lgnumalloc
DPADD+= /usr/lib/libgnumalloc.a

SUBDIR=	ldconfig ldd rtld

.PATH: $(.CURDIR)/$(MACHINE)

.include <bsd.prog.mk>
