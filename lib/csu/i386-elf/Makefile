#
# $FreeBSD$
#

SRCS=		crt1.c crti.S crtn.S
OBJS=		${SRCS:N*.h:R:S/$/.o/g}
OBJS+=		gcrt1.o
CFLAGS+=	-elf -Wall -fkeep-inline-functions \
		-I${.CURDIR}/../common
LDFLAGS+=	-elf

.PATH:		${.CURDIR}/../common

all: ${OBJS}

CLEANFILES=	${OBJS}

gcrt1.o:	crt1.c
	${CC} ${CFLAGS} -DGCRT -c -o gcrt1.o ${.CURDIR}/crt1.c

realinstall:
.for file in ${OBJS}
	${INSTALL} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	    ${OBJS} ${DESTDIR}${LIBDIR}
.endfor

.include <bsd.lib.mk>
