#
#	$Id: Makefile,v 1.3 1998/03/11 20:41:55 jb Exp $
#

SRCS=		crt1.c crtbegin.c crtend.c
OBJS=		crt1.o crtbegin.o crtend.o
SOBJS=		crtbegin.so crtend.so
CFLAGS+=	-Wall -Wno-unused
NOMAN=		true
NOPIC=		true
NOPROFILE=	true
INTERNALLIB=	true
CFLAGS=

.ifndef	BOOTSTRAP
CFLAGS+=	-DHAVE_RTLD -I${.CURDIR}/../../../libexec/rtld-elf
.endif

all: ${OBJS} ${SOBJS}

realinstall:
.for file in ${OBJS} ${SOBJS}
	${INSTALL} ${COPY} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	    ${file} ${DESTDIR}${LIBDIR}/${file:S/.so$/S.o/}
.endfor

.include <bsd.lib.mk>
