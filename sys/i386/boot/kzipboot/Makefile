# $Id: Makefile,v 1.6 1997/02/22 09:31:22 peter Exp $

PROG= kztail.o kzhead.o
BINMODE = 444	# target is a relocatable object
SRCS= tail.S head.S boot.c unzip.c misc.c malloc.c inflate.c
OBJS_KZHEAD= head.o
OBJS_KZTAIL= tail.o boot.o unzip.o misc.o malloc.o inflate.o
BINDIR= /usr/lib
.PATH:	${.CURDIR}/../../../kern
NOMAN= toobad

# Where to load the kernel
KADDR   = 0x100000

# What segment our code lives in
CSEG    = 0x8

STRIP=	#  very important!! don't let kz*.o be stripped

.if exists(${.CURDIR}/../../../../include)
CFLAGS+= -nostdinc
CFLAGS+= -I${.OBJDIR} -I${.CURDIR}/../../../
CFLAGS+= -I${.CURDIR}/../../../../include
.endif

CFLAGS+= -DKADDR=$(KADDR) -DCSEG=$(CSEG)
CFLAGS+= -DKZIP -DCOMCONSOLE=0x3F8

CLEANFILES=machine

kztail.o:  ${OBJS_KZTAIL}
	$(LD) -r -x -o kztail.o $(OBJS_KZTAIL)

kzhead.o:  ${OBJS_KZHEAD}
	$(LD) -r -x -o kzhead.o $(OBJS_KZHEAD)

pre-compile:
.if !exists(${.OBJDIR}/machine)
	ln -s ${.CURDIR}/../../include ${.OBJDIR}/machine
.endif

all:	pre-compile ${PROG}


.include <bsd.prog.mk>
