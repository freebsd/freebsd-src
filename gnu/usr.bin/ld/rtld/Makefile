#	$Id: Makefile,v 1.5 1993/11/22 19:05:26 jkh Exp $

PROG=	ld.so
SRCS=	mdprologue.S rtld.c shlib.c etc.c md.c
NOMAN=	noman
LDDIR?= $(.CURDIR)/..
#PICFLAG=-pic
PICFLAG=-fpic
CFLAGS += -I$(LDDIR) -I$(.CURDIR) -I$(LDDIR)/$(MACHINE) -O $(PICFLAG) -DRTLD
LDFLAGS = -Bshareable -Bsymbolic -assert nosymbolic
LIBS =  -lc_pic -lgcc_pic
BINDIR= /usr/libexec

.PATH: $(LDDIR) $(LDDIR)/$(MACHINE)

.SUFFIXES: .S

$(PROG):
	$(LD) -o $(PROG) $(LDFLAGS) $(OBJS) $(LIBS) $(LDADD)

.S.o:
	$(CPP) $(.IMPSRC) | $(AS) -k -o $(.TARGET) -

.include <bsd.prog.mk>
