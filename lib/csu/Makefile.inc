# $FreeBSD$

SSP_CFLAGS=

NO_WMISSING_VARIABLE_DECLARATIONS=

.include <src.opts.mk>

.if !defined(BUILDING_TESTS)

OBJS+=	Scrt1.o crt1.o gcrt1.o
OBJS+=	crtbegin.o crtbeginS.o crtbeginT.o
OBJS+=	crtend.o crtendS.o
OBJS+=	crti.o crtn.o

CRT1OBJS+=	crtbrand.o ignore_init_note.o

ACFLAGS+=	-DLOCORE

CFLAGS+=	-fno-asynchronous-unwind-tables
CFLAGS+=	-fno-omit-frame-pointer
CFLAGS+=	-I${.CURDIR:H}/common \
		-I${SRCTOP}/lib/libc/include

CFLAGS_CRTS=	-DSHARED ${PICFLAG}

FILES=		${OBJS}
FILESMODE=	${LIBMODE}
FILESOWN=	${LIBOWN}
FILESGRP=	${LIBGRP}
FILESDIR=	${LIBDIR}
# These FILES qualify as libraries for the purpose of LIBRARIES_ONLY.
.undef LIBRARIES_ONLY

CLEANFILES+=	${OBJS} ${CRT1OBJS} crt1_c.o gcrt1_c.o Scrt1_c.o

crt1.o:	crt1_c.o ${CRT1OBJS}
	${LD} ${_LDFLAGS} -o ${.TARGET} -r ${.ALLSRC:M*.o}
.if ${MACHINE_ARCH} == "i386"
	${OBJCOPY} --localize-symbol _start1 ${.TARGET}
.endif

gcrt1_c.o: crt1_c.c
	${CC} ${CFLAGS} -DGCRT -c -o ${.TARGET} ${.CURDIR}/crt1_c.c

gcrt1.o: gcrt1_c.o ${CRT1OBJS}
	${LD} ${_LDFLAGS} -o ${.TARGET} -r ${.ALLSRC:M*.o}

Scrt1_c.o: crt1_c.c
	${CC} ${CFLAGS} -fPIC -DPIC -c -o ${.TARGET} ${.CURDIR}/crt1_c.c

Scrt1.o: Scrt1_c.o ${CRT1OBJS}
	${LD} ${_LDFLAGS} -o ${.TARGET} -r ${.ALLSRC:M*.o}
.if ${MACHINE_ARCH} == "i386"
	${OBJCOPY} --localize-symbol _start1 ${.TARGET}
.endif

crtbegin.o: crtbegin.c
crtbeginS.o: crtbegin.c
crtbeginT.o: crtbegin.c
crtend.o: crtend.c
crtendS.o: crtend.c

crtbegin.o crtend.o crtbeginT.o:
	${CC} ${CFLAGS} -I${.CURDIR} -c -o ${.TARGET} ${.ALLSRC:N*.h:[1]}

crtbeginS.o crtendS.o:
	${CC} ${CFLAGS} -I${.CURDIR} ${CFLAGS_CRTS} -c -o ${.TARGET} \
	    ${.ALLSRC:N*.h:[1]}

.endif

.include "../Makefile.inc"
