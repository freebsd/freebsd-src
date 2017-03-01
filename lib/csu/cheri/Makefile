#
# $FreeBSD$
#
SRCS=	crt_invoke.S				\
	crt_rtld.S				\
	crtbeginC.c				\
	crtendC.c
FILES=	crt_invoke.o				\
	crt_rtld.o				\
	crtbeginC.o				\
	crtendC.o
FILESOWN=       ${LIBOWN}
FILESGRP=       ${LIBGRP}
FILESMODE=      ${LIBMODE}
FILESDIR=       ${LIBDIR}
# These FILES qualify as libraries for the purpose of LIBRARIES_ONLY.
.undef LIBRARIES_ONLY
CLEANFILES=	${FILES}
CFLAGS+=-mxgot -mllvm -mxmxgot

NEED_CHERI=	pure

.if defined(LIBCHERI)
CFLAGS+=	-I${.CURDIR}/../common		\
		-I${.CURDIR}/../../libc/include
FILES+=	crt1.o					\
	Scrt1.o					\
	crtbegin.o				\
	crtbeginS.o				\
	crtbeginT.o				\
	crtend.o				\
	crtendS.o

crtbegin.o:	crtbeginC.c
	${CC} ${CFLAGS} -g0 -c -o ${.TARGET} ${.ALLSRC}

crtbeginS.o:	crtbeginC.c
	${CC} ${CFLAGS} ${PICFLAG} -g0 -c -o ${.TARGET} ${.ALLSRC}

crtbeginT.o:	crtbeginC.c
	${CC} ${CFLAGS} -g0 -c -o ${.TARGET} ${.ALLSRC}

crtendS.o:	crtendC.c
	${CC} ${CFLAGS} ${PICFLAG} -g0 -c -o ${.TARGET} ${.ALLSRC}

crtend.o:	crtendC.c
	${CC} ${CFLAGS} -g0 -c -o ${.TARGET} ${.ALLSRC}

Scrt1.o:	crt1.c
	${CC} ${CFLAGS} ${PICFLAG} -g0 -c -o ${.TARGET} ${.ALLSRC}

.endif

.include <bsd.lib.mk>
