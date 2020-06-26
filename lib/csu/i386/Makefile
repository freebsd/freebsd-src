# $FreeBSD$

.PATH: ${.CURDIR:H}/common

SRCS=		crti.S crtn.S
OBJS=		${SRCS:N*.h:R:S/$/.o/g}
OBJS+=		Scrt1.o crt1.o gcrt1.o
CFLAGS+=	-I${.CURDIR} -I${.CURDIR:H}/common \
		-I${SRCTOP}/lib/libc/include
CFLAGS+=	-DCRT_IRELOC_REL

FILES=		${OBJS}
FILESMODE=	${LIBMODE}
FILESOWN=	${LIBOWN}
FILESGRP=	${LIBGRP}
FILESDIR=	${LIBDIR}
# These FILES qualify as libraries for the purpose of LIBRARIES_ONLY.
.undef LIBRARIES_ONLY

CLEANFILES=	${OBJS} crt1_c.o crt1_s.o gcrt1_c.o Scrt1_c.o
CLEANFILES+=	crtbrand.o ignore_init_note.o

gcrt1_c.o: crt1_c.c
	${CC} ${CFLAGS} -DGCRT -c -o ${.TARGET} ${.CURDIR}/crt1_c.c

gcrt1.o: gcrt1_c.o crt1_s.o crtbrand.o ignore_init_note.o
	${LD} ${_LDFLAGS} -o ${.TARGET} -r ${.ALLSRC:M*.o}

crt1.o:	crt1_c.o crt1_s.o crtbrand.o ignore_init_note.o
	${LD} ${_LDFLAGS} -o ${.TARGET} -r ${.ALLSRC:M*.o}
	${OBJCOPY} --localize-symbol _start1 crt1.o

Scrt1_c.o: crt1_c.c
	${CC} ${CFLAGS} -fPIC -DPIC -c -o ${.TARGET} ${.CURDIR}/crt1_c.c

Scrt1.o: Scrt1_c.o crt1_s.o crtbrand.o ignore_init_note.o
	${LD} ${_LDFLAGS} -o ${.TARGET} -r ${.ALLSRC:M*.o}
	${OBJCOPY} --localize-symbol _start1 Scrt1.o

.include <bsd.lib.mk>
