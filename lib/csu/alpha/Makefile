#
#	$Id: Makefile,v 1.1.1.1 1998/03/07 20:27:10 jdp Exp $
#

.PATH:		${.CURDIR}/../i386-elf
SRCS=		crt1.c crtbegin.c crtend.c crti.S crtn.S
OBJS=		${SRCS:N*.h:R:S/$/.o/g}
SOBJS=		crtbegin.so crtend.so
CFLAGS+=	-Wall -Wno-unused
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
