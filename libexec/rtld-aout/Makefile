#	$Id: Makefile,v 1.14 1995/03/04 17:46:23 nate Exp $

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
INSTALLFLAGS+=	-fschg
MLINKS= rtld.1 ld.so.1

.PATH: $(LDDIR) $(LDDIR)/$(MACHINE)

$(PROG): ${OBJS} ${DPADD}
	$(LD) -o $(PROG) $(LDFLAGS) $(OBJS) $(LDADD)

.include <bsd.prog.mk>
