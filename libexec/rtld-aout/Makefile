#	$Id: Makefile,v 1.13 1994/09/18 19:41:38 swallace Exp $

PROG=	ld.so
SRCS=	mdprologue.S rtld.c malloc.c shlib.c etc.c md.c
NOMAN=	noman
LDDIR?= $(.CURDIR)/..
PICFLAG=-fpic
CFLAGS+=-I$(LDDIR) -I$(.CURDIR) -I$(LDDIR)/$(MACHINE) $(PICFLAG) -DRTLD
LDFLAGS+=-Bshareable -Bsymbolic -assert nosymbolic
ASFLAGS+=-k
DPADD+=	${LIBC:S/c.a/c_pic.a/} ${LIBC:S/c.a/gcc_pic.a/}
LDADD+=	-lc_pic -lgcc_pic
BINDIR= /usr/libexec
INSTALLFLAGS+=	-fschg

.PATH: $(LDDIR) $(LDDIR)/$(MACHINE)

$(PROG): ${OBJS} ${DPADD}
	$(LD) -o $(PROG) $(LDFLAGS) $(OBJS) $(LDADD)

.include <bsd.prog.mk>
