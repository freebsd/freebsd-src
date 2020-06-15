# $FreeBSD$

SSP_CFLAGS=

NO_WMISSING_VARIABLE_DECLARATIONS=

.include <src.opts.mk>

.if !defined(BUILDING_TESTS)

OBJS+=	crtbegin.o crtbeginS.o crtbeginT.o
OBJS+=	crtend.o crtendS.o

ACFLAGS+=	-DLOCORE

CFLAGS+=	-fno-asynchronous-unwind-tables
CFLAGS+=	-fno-omit-frame-pointer

CFLAGS_CRTS=	-DSHARED ${PICFLAG}

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
