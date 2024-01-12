PACKAGE=	clibs-dev

NO_WMISSING_VARIABLE_DECLARATIONS=
# Can't instrument these files since that breaks non-sanitized programs.
MK_ASAN:=	no
MK_UBSAN:=	no

.include <src.opts.mk>
MK_SSP=	no

.if !defined(BUILDING_TESTS)

OBJS+=	Scrt1.o crt1.o gcrt1.o
OBJS+=	crtbegin.o crtbeginS.o crtbeginT.o
OBJS+=	crtend.o crtendS.o
OBJS+=	crti.o crtn.o

CRT1OBJS+=	crtbrand.o feature_note.o ignore_init_note.o

ACFLAGS+=	-DLOCORE

CFLAGS+=	-DSTRIP_FBSDID
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

CRT1SRC?=	crt1_c.c
CRT1OBJ?=

CLEANFILES+=	${OBJS} ${CRT1OBJS} crt1_c.o gcrt1_c.o Scrt1_c.o
CLEANFILES+=	crti_s.o ${CRT1SRC:C/.[S|c]$/.o/}

crt1.o:	${CRT1SRC:C/.[S|c]$/.o/} ${CRT1OBJS} ${CRT1OBJ}
	${LD} ${_LDFLAGS} -o ${.TARGET} -r ${.ALLSRC:M*.o}

gcrt1_c.o: ${CRT1SRC}
	${CC} ${CFLAGS} -DGCRT -c -o ${.TARGET} ${.CURDIR}/${CRT1SRC}

gcrt1.o: gcrt1_c.o ${CRT1OBJS} ${CRT1OBJ}
	${LD} ${_LDFLAGS} -o ${.TARGET} -r ${.ALLSRC:M*.o}

Scrt1_c.o: ${CRT1SRC}
	${CC} ${CFLAGS} -fPIC -DPIC -c -o ${.TARGET} ${.CURDIR}/${CRT1SRC}

Scrt1.o: Scrt1_c.o ${CRT1OBJS} ${CRT1OBJ}
	${LD} ${_LDFLAGS} -o ${.TARGET} -r ${.ALLSRC:M*.o}

# __FreeBSD_version is recorded in crt1.o et al via crtbrand.
crtbrand.o: ${SRCTOP}/sys/sys/param.h

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

crti_s.o: crti.S
	${CC} ${CFLAGS} ${ACFLAGS} -c ${.ALLSRC:M*.S} -o ${.TARGET}

crti.o: crti_s.o crtbrand.o
	${LD} ${_LDFLAGS} -o ${.TARGET} -r ${.ALLSRC:M*.o}

.endif

.include "../Makefile.inc"
