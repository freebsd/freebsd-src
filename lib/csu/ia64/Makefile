# $FreeBSD$

.PATH: ${.CURDIR}/../common

SRCS=		crti.S crtn.S
OBJS=		${SRCS:N*.h:R:S/$/.o/g}
OBJS+=		crt1.o gcrt1.o Scrt1.o

all: ${OBJS}

CLEANFILES=	${OBJS}
CLEANFILES+=	crt1_.o gcrt1_.o Scrt1_.o
CLEANFILES+=	crtbrand.o gcrtbrand.o Scrtbrand.o
CLEANFILES+=	crtbrand.s gcrtbrand.s Scrtbrand.s

crt1_.o: crt1.S
	${CC} ${CFLAGS} -c -o ${.TARGET} ${.ALLSRC}

# See the comment in lib/csu/common/crtbrand.c for the reason crtbrand.c is not
# directly compiled to .o files.

crtbrand.s: crtbrand.c
	${CC} ${CFLAGS} -S -o ${.TARGET} ${.ALLSRC}
	sed -i "" -e '/\.note\.ABI-tag/s/progbits/note/' ${.TARGET}

crtbrand.o: crtbrand.s
	${CC} ${ACFLAGS} -c -o ${.TARGET} crtbrand.s

crt1.o: crt1_.o crtbrand.o
	${LD} ${LDFLAGS} -r -o ${.TARGET} crt1_.o crtbrand.o

gcrt1_.o: crt1.S
	${CC} ${CFLAGS} -DGCRT -c -o ${.TARGET} ${.ALLSRC}

gcrtbrand.s: crtbrand.c
	${CC} ${CFLAGS} -DGCRT -S -o ${.TARGET} ${.ALLSRC}
	sed -i "" -e '/\.note\.ABI-tag/s/progbits/note/' ${.TARGET}

gcrtbrand.o: gcrtbrand.s
	${CC} ${ACFLAGS} -c -o ${.TARGET} gcrtbrand.s

gcrt1.o: gcrt1_.o gcrtbrand.o
	${LD} ${LDFLAGS} -r -o ${.TARGET} ${.ALLSRC}

Scrt1_.o: crt1.S
	${CC} ${CFLAGS} -fPIC -DPIC -c -o ${.TARGET} ${.ALLSRC}

Scrtbrand.s: crtbrand.c
	${CC} ${CFLAGS} -fPIC -DPIC -S -o ${.TARGET} ${.ALLSRC}
	sed -i "" -e '/\.note\.ABI-tag/s/progbits/note/' ${.TARGET}

Scrtbrand.o: Scrtbrand.s
	${CC} ${ACFLAGS} -c -o ${.TARGET} Scrtbrand.s

Scrt1.o: Scrt1_.o Scrtbrand.o
	${LD} ${LDFLAGS} -r -o ${.TARGET} ${.ALLSRC}

realinstall:
	${INSTALL} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	    ${OBJS} ${DESTDIR}${LIBDIR}

.include <bsd.lib.mk>
