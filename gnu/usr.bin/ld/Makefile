# $Id: Makefile,v 1.21 1997/02/22 15:46:17 peter Exp $
#

PROG=	ld
SRCS=	ld.c symbol.c lib.c shlib.c warnings.c support.c rrs.c xbits.c md.c \
	cplus-dem.c
CFLAGS+= -I$(.CURDIR) -I$(.CURDIR)/$(MACHINE) \
	-I$(GCCDIR) -DIN_GCC -DDEMANGLE_CPLUSPLUS
NOSHARED?= yes

GCCDIR=	${.CURDIR}/../../../contrib/gcc

SUBDIR=       ldconfig ldd
.if !defined(NOPIC)
SUBDIR+= rtld
.endif

.PATH: $(.CURDIR)/$(MACHINE) $(GCCDIR)

.include <bsd.prog.mk>
