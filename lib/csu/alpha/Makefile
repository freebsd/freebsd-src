#
#	$Id: Makefile,v 1.6 1998/08/20 21:37:22 jb Exp $
#

SRCS=		crt1.c crtbegin.c crtend.c
OBJS=		crt1.o crtbegin.o crtend.o
SOBJS=		crtbegin.so crtend.so
CFLAGS+=	-Wall -Wno-unused -I${.CURDIR}/../../../libexec/rtld-elf
NOMAN=		true
NOPIC=		true
NOPROFILE=	true
INTERNALLIB=	true

all: ${OBJS} ${SOBJS}

realinstall:
.for file in ${OBJS} ${SOBJS}
	${INSTALL} ${COPY} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	    ${file} ${DESTDIR}${LIBDIR}/${file:S/.so$/S.o/}
.endfor

.include <bsd.lib.mk>
