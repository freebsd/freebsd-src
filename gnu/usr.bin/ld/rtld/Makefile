#	$Id: Makefile,v 1.20 1996/09/12 03:42:54 bde Exp $

PROG=	ld.so
SRCS=	mdprologue.S rtld.c malloc.c shlib.c md.c support.c sbrk.c
MAN1=	rtld.1
LDDIR?= $(.CURDIR)/..
# As there is relocation going on behind GCC's back, don't cache function addresses.
PICFLAG=-fpic -fno-function-cse
CFLAGS+=-I$(LDDIR) -I$(.CURDIR) -I$(LDDIR)/$(MACHINE) $(PICFLAG) -DRTLD
LDFLAGS+=-Bshareable -Bsymbolic -assert nosymbolic
ASFLAGS+=-k
DPADD+=	${LIBC:S/c.a/c_pic.a/} ${LIBC:S/c.a/gcc_pic.a/}
LDADD+=	-lc_pic -lgcc_pic
BINDIR= /usr/libexec
INSTALLFLAGS+=	-fschg -C	# -C to install as atomically as possible
MLINKS= rtld.1 ld.so.1

.PATH: $(LDDIR) $(LDDIR)/$(MACHINE)

.if defined(DESTDIR)
$(PROG):
	$(LD) -o $(PROG) $(LDFLAGS) -nostdlib -L${DESTDIR}/usr/lib $(OBJS) $(LDADD)
.else
$(PROG):
	$(LD) -o $(PROG) $(LDFLAGS) $(OBJS) $(LDADD)
.endif

.include <bsd.prog.mk>
