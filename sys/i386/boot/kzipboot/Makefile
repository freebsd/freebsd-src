# $Id: Makefile,v 1.2.4.2 1995/10/04 09:24:19 jkh Exp $

PROG= kzip.o
BINMODE = 444	# target is a relocatable object
SRCS= head.S boot.c unzip.c misc.c malloc.c inflate.c
BINDIR= /usr/lib
.PATH:	${.CURDIR}/../../../kern
NOMAN= toobad

# Where to load the kernel
KADDR   = 0x100000

# What segment our code lives in
CSEG    = 0x8

STRIP=	#  very important!! don't let kzip.o be stripped

CFLAGS+= -DKADDR=$(KADDR) -DCSEG=$(CSEG)
CFLAGS+= -DKZIP -DCOMCONSOLE=0x3F8

kzip.o:  ${OBJS}
	$(LD) -r -x -o kzip.o $(OBJS)

.include <bsd.prog.mk>
