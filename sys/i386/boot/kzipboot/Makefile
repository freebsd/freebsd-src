# $Id: Makefile,v 1.8 1998/03/07 10:48:10 eivind Exp $

PROG= kztail.o kzhead.o
BINMODE = 444	# target is a relocatable object
SRCS= tail.S head.S boot.c unzip.c misc.c malloc.c inflate.c
OBJS_KZHEAD= head.o
OBJS_KZTAIL= tail.o boot.o unzip.o misc.o malloc.o inflate.o
BINDIR= /usr/lib/aout
.PATH:	${.CURDIR}/../../../kern
NOMAN= toobad

# Where to load the kernel
KADDR   = 0x100000

# What segment our code lives in
CSEG    = 0x8

STRIP=	#  very important!! don't let kz*.o be stripped

CFLAGS+= -DKADDR=$(KADDR) -DCSEG=$(CSEG)
CFLAGS+= -DKZIP -DCOMCONSOLE=0x3F8

kztail.o:  ${OBJS_KZTAIL}
	$(LD) -r -x -o kztail.o $(OBJS_KZTAIL)

kzhead.o:  ${OBJS_KZHEAD}
	$(LD) -r -x -o kzhead.o $(OBJS_KZHEAD)

.include <bsd.prog.mk>
