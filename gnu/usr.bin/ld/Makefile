# $Id: Makefile,v 1.10 1993/12/01 15:58:22 ache Exp $
#

PROG=	ld
SRCS=	ld.c symbol.c lib.c shlib.c warnings.c etc.c rrs.c xbits.c md.c
CFLAGS += -static -I$(.CURDIR) -I$(.CURDIR)/$(MACHINE)

LDADD+=	-lgnumalloc
DPADD+= /usr/lib/libgnumalloc.a
LDFLAGS+= -Xlinker -Bstatic

SUBDIR=       ldconfig ldd
.if !defined(NOPIC)
SUBDIR+= rtld
.endif

.PATH: $(.CURDIR)/$(MACHINE)

.include <bsd.prog.mk>
