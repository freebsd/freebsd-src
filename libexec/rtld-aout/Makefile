#	$Id: Makefile,v 1.24 1997/04/16 11:31:32 bde Exp $
PROG=	ld.so
SRCS=	mdprologue.S rtld.c shlib.c md.c support.c
MAN1=	rtld.1
LDDIR?= $(.CURDIR)/..
# As there is relocation going on behind GCC's back, don't cache function addresses.
PICFLAG=-fpic -fno-function-cse
CFLAGS+=-I$(LDDIR) -I$(.CURDIR) -I$(LDDIR)/$(MACHINE) $(PICFLAG) -DRTLD -Wall
LDFLAGS+=-nostdlib -Wl,-Bshareable,-Bsymbolic,-assert,nosymbolic
ASFLAGS+=-k
DPADD+=	${LIBC:S/c.a/c_pic.a/} ${LIBC:S/c.a/gcc_pic.a/}
LDADD+=	-lc_pic -lgcc_pic
BINDIR= /usr/libexec
INSTALLFLAGS+=	-fschg -C	# -C to install as atomically as possible
MLINKS= rtld.1 ld.so.1

.PATH: $(LDDIR) $(LDDIR)/$(MACHINE)

.include <bsd.prog.mk>
