#	$Id: Makefile,v 1.19 1996/05/07 23:15:58 wosch Exp $

PROG=	ld.so
SRCS=	mdprologue.S rtld.c malloc.c shlib.c etc.c md.c
MAN1=	rtld.1
LDDIR?= $(.CURDIR)/..
PICFLAG=-fpic
CFLAGS+=-I$(LDDIR) -I$(.CURDIR) -I$(LDDIR)/$(MACHINE) $(PICFLAG) -DRTLD
LDFLAGS+=-Bshareable -Bsymbolic -assert nosymbolic
ASFLAGS+=-k
DPADD+=	${LIBC:S/c.a/c_pic.a/} ${LIBC:S/c.a/gcc_pic.a/}
LDADD+=	-lc_pic -lgcc_pic
BINDIR= /usr/libexec
INSTALLFLAGS+=	-fschg -C	# -C to install as atomically as possible
MLINKS= rtld.1 ld.so.1

.PATH: $(LDDIR) $(LDDIR)/$(MACHINE)

$(PROG): ${OBJS} ${DPADD}
	$(LD) -o $(PROG) $(LDFLAGS) $(OBJS) $(LDADD)

.include <bsd.prog.mk>
