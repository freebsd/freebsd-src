#	$Id: Makefile,v 1.8 1994/01/28 21:01:20 pk Exp $

PROG=	ld.so
SRCS=	mdprologue.S rtld.c malloc.c shlib.c etc.c md.c
NOMAN=	noman
LDDIR?= $(.CURDIR)/..
#PICFLAG=-pic
PICFLAG=-fpic
CFLAGS+=-I$(LDDIR) -I$(.CURDIR) -I$(LDDIR)/$(MACHINE) $(PICFLAG) -DRTLD
LDFLAGS+=-Bshareable -Bsymbolic -assert nosymbolic
ASFLAGS+=-k
LDADD+=	-lc_pic
BINDIR= /usr/libexec

.SUFFIXES: .S

.PATH: $(LDDIR) $(LDDIR)/$(MACHINE)

$(PROG):
	$(LD) -o $(PROG) $(LDFLAGS) $(OBJS) $(LDADD)

.S.o:
	${CPP} ${.IMPSRC} | ${AS} ${ASFLAGS} -o ${.TARGET} -

.include <bsd.prog.mk>
