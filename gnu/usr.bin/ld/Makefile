# $FreeBSD$
#

PROG=	ld
SRCS=	ld.c symbol.c lib.c shlib.c warnings.c etc.c rrs.c xbits.c md.c
CFLAGS += -I$(.CURDIR) -I$(.CURDIR)/$(MACHINE)
LDFLAGS+= -Xlinker -Bstatic

DPADD+=	$(LIBGNUMALLOC)
LDADD+=	-lgnumalloc

SUBDIR=       ldconfig ldd
.if !defined(NOPIC)
SUBDIR+= rtld
.endif

.PATH: $(.CURDIR)/$(MACHINE)

.include <bsd.prog.mk>
