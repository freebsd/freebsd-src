#
#	$Id: Makefile,v 1.7 1999/01/09 21:50:56 jdp Exp $
#

RTLDDIR=	${.CURDIR}/../../../libexec/rtld-elf

SRCS=		crt1.c crtbegin.c crtend.c
OBJS=		crt1.o crtbegin.o crtend.o
SOBJS=		crtbegin.So crtend.So
CFLAGS+=	-Wall -Wno-unused -I${RTLDDIR}/${MACHINE_ARCH} -I${RTLDDIR}
NOMAN=		true
NOPIC=		true
NOPROFILE=	true
INTERNALLIB=	true

all: ${OBJS} ${SOBJS}

realinstall:
.for file in ${OBJS} ${SOBJS}
	${INSTALL} ${COPY} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	    ${file} ${DESTDIR}${LIBDIR}/${file:S/.So$/S.o/}
.endfor

.include <bsd.lib.mk>
