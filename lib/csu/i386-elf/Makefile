#
#	$Id: Makefile,v 1.4 1999/03/12 17:33:26 jdp Exp $
#

SRCS=		crt1.c crtbegin.c crtend.c crti.S crtn.S
OBJS=		${SRCS:N*.h:R:S/$/.o/g}
OBJS+=		gcrt1.o
SOBJS=		crtbegin.So crtend.So
CFLAGS+=	-elf
CFLAGS+=	-Wall
LDFLAGS+=	-elf
NOMAN=		true
NOPIC=		true
NOPROFILE=	true
INTERNALLIB=	true

all: ${OBJS} ${SOBJS}

gcrt1.o:	crt1.c
	${CC} ${CFLAGS} -DGCRT -c -o gcrt1.o ${.CURDIR}/crt1.c

realinstall:
.for file in ${OBJS} ${SOBJS}
	${INSTALL} ${COPY} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	    ${file} ${DESTDIR}${LIBDIR}/${file:S/.So$/S.o/}
.endfor

.include <bsd.lib.mk>
