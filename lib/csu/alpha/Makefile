#
#	$Id: Makefile,v 1.4 1998/04/01 03:24:19 jb Exp $
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

# Force bootstrap mode until rtld is ported. The headers are broken
# on alpha as the result of i386 changes.
BOOTSTRAP=	true

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
