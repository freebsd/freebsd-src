# $Id: Makefile,v 1.18 1996/10/01 01:22:12 peter Exp $
#

PROG=	ld
SRCS=	ld.c symbol.c lib.c shlib.c warnings.c support.c rrs.c xbits.c md.c cplus-dem.c
CFLAGS += -I$(.CURDIR) -I$(.CURDIR)/$(MACHINE) -I$(GCCDIR) -DIN_GCC -DDEMANGLE_CPLUSPLUS
LDFLAGS+= -Xlinker -Bstatic

GCCDIR=	${.CURDIR}/../../../contrib/gcc

SUBDIR=       ldconfig ldd
.if !defined(NOPIC)
SUBDIR+= rtld
.endif

.PATH: $(.CURDIR)/$(MACHINE) $(GCCDIR)

.include <bsd.prog.mk>
