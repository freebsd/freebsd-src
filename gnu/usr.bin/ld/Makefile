# $Id: Makefile,v 1.22 1997/06/29 21:36:33 bde Exp $
#

PROG=	ld
BINDIR= /usr/libexec/aout
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
