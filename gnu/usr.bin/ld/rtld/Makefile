#	$Id: Makefile,v 1.23 1997/02/22 15:46:46 peter Exp $

PROG=	ld.so
SRCS=	mdprologue.S rtld.c malloc.c shlib.c md.c support.c sbrk.c
MAN1=	rtld.1
LDDIR?= $(.CURDIR)/..
# As there is relocation going on behind GCC's back, don't cache function addresses.
PICFLAG=-fpic -fno-function-cse
CFLAGS+=-I$(LDDIR) -I$(.CURDIR) -I$(LDDIR)/$(MACHINE) $(PICFLAG) -DRTLD
LDFLAGS+=-nostdlib -Wl,-Bshareable -Wl,-Bsymbolic -Wl,-assert -Wl,nosymbolic
ASFLAGS+=-k
DPADD+=	${LIBC:S/c.a/c_pic.a/} ${LIBC:S/c.a/gcc_pic.a/}
LDADD+=	-lc_pic -lgcc_pic
BINDIR= /usr/libexec
INSTALLFLAGS+=	-fschg -C	# -C to install as atomically as possible
MLINKS= rtld.1 ld.so.1

.PATH: $(LDDIR) $(LDDIR)/$(MACHINE)

.include <bsd.prog.mk>
