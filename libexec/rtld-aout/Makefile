#	$Id: Makefile,v 1.7 1993/12/11 21:05:59 jkh Exp $

PROG=	ld.so
SRCS=	mdprologue.S sbrk.c rtld.c shlib.c etc.c md.c
NOMAN=	noman
LDDIR?= $(.CURDIR)/..
#PICFLAG=-pic
PICFLAG=-fpic
CFLAGS += -I$(LDDIR) -I$(.CURDIR) -I$(LDDIR)/$(MACHINE) -O $(PICFLAG) -DRTLD
LDFLAGS = -Bshareable -Bsymbolic -assert nosymbolic -Z
LIBS =  -lc_pic -lgcc_pic
BINDIR= /usr/libexec
ASFLAGS = -k

.PATH: $(LDDIR) $(LDDIR)/$(MACHINE)

.SUFFIXES: .S

$(PROG):
	$(LD) -o $(PROG) $(LDFLAGS) $(OBJS) $(LIBS) $(LDADD)

.S.o:
	$(CPP) $(.IMPSRC) | $(AS) $(ASFLAGS) -o $(.TARGET) -

.include <bsd.prog.mk>
