# $Id: Makefile,v 1.9 1993/12/01 14:45:35 ache Exp $
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
