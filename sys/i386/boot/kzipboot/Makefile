# $Id: Makefile,v 1.2.4.1 1995/08/23 04:41:23 davidg Exp $

PROG= kzip.o
BINMODE = 444	# target is a relocatable object
SRCS= kzhead.S head.S boot.c unzip.c misc.c malloc.c inflate.c

KOBJS= head.o boot.o unzip.o misc.o malloc.o inflate.o

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

beforeinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${.CURDIR}/obj/kzhead.o ${DESTDIR}${BINDIR}/kzhead.o

kzip.o:  ${KOBJS}
	$(LD) -r -x -o kzip.o $(KOBJS)

.include <bsd.prog.mk>
