# $Id: Makefile,v 1.17 1995/09/22 14:14:32 phk Exp $
#

PROG=	ld
SRCS=	ld.c symbol.c lib.c shlib.c warnings.c support.c rrs.c xbits.c md.c
CFLAGS += -I$(.CURDIR) -I$(.CURDIR)/$(MACHINE)
LDFLAGS+= -Xlinker -Bstatic

SUBDIR=       ldconfig ldd
.if !defined(NOPIC)
SUBDIR+= rtld
.endif

.PATH: $(.CURDIR)/$(MACHINE)

.include <bsd.prog.mk>
