#	$Id: Makefile,v 1.4 1993/11/09 04:44:29 paul Exp $

PROG=	ld.so
SRCS=	mdprologue.S rtld.c shlib.c etc.c md.c
NOMAN=	noman
LDDIR?= $(.CURDIR)/..
#PICFLAG=-pic
PICFLAG=-fpic
CFLAGS += -I$(LDDIR) -I$(.CURDIR) -I$(LDDIR)/$(MACHINE) -O $(PICFLAG) -DRTLD
LDFLAGS = -Bshareable -Bsymbolic -assert nosymbolic
LIBS =  -lc_pic
BINDIR= /usr/libexec

.PATH: $(LDDIR) $(LDDIR)/$(MACHINE)

.SUFFIXES: .S

$(PROG):
	$(LD) -o $(PROG) $(LDFLAGS) $(OBJS) $(LIBS) $(LDADD)

.S.o:
	$(CPP) $(.IMPSRC) | $(AS) -k -o $(.TARGET) -

.include <bsd.prog.mk>
