# $FreeBSD$

.PATH: ${.CURDIR}/../common

SRCS=		crti.S crtn.S
OBJS=		${SRCS:N*.h:R:S/$/.o/g}
OBJS+=		crt1.o gcrt1.o Scrt1.o
CFLAGS+=	-Wall -Wno-unused

all: ${OBJS}

CLEANFILES=	${OBJS}
CLEANFILES+=	crt1_.o gcrt1_.o Scrt1_.o
CLEANFILES+=	crtbrand.o gcrtbrand.o Scrtbrand.o

crt1_.o: crt1.S
	${CC} ${CFLAGS} -c -o ${.TARGET} ${.ALLSRC}

crtbrand.o: crtbrand.c
	${CC} ${CFLAGS} -c -o ${.TARGET} ${.ALLSRC}

crt1.o: crt1_.o crtbrand.o
	${LD} ${LDFLAGS} -r -o ${.TARGET} crt1_.o crtbrand.o

gcrt1_.o: crt1.S
	${CC} ${CFLAGS} -DGCRT -c -o ${.TARGET} ${.ALLSRC}

gcrtbrand.o: crtbrand.c
	${CC} ${CFLAGS} -DGCRT -c -o ${.TARGET} ${.ALLSRC}

gcrt1.o: gcrt1_.o gcrtbrand.o
	${LD} ${LDFLAGS} -r -o ${.TARGET} ${.ALLSRC}

Scrt1_.o: crt1.S
	${CC} ${CFLAGS} -fPIC -DPIC -c -o ${.TARGET} ${.ALLSRC}

Scrtbrand.o: crtbrand.c
	${CC} ${CFLAGS} -fPIC -DPIC -c -o ${.TARGET} ${.ALLSRC}

Scrt1.o: Scrt1_.o Scrtbrand.o
	${LD} ${LDFLAGS} -r -o ${.TARGET} ${.ALLSRC}

realinstall:
	${INSTALL} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	    ${OBJS} ${DESTDIR}${LIBDIR}

.include <bsd.lib.mk>
