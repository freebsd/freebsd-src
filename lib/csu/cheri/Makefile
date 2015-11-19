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
CLEANFILES=	${FILES}
CFLAGS+=-mxgot

NEED_CHERI=	pure

.if defined(LIBCHERI)
FILES+=	crtbegin.o				\
	crtbeginT.o				\
	crtend.o

crtbegin.o:	crtbeginC.c
	${CC} ${CFLAGS} -g0 -c -o ${.TARGET} ${.ALLSRC}

crtbeginT.o:	crtbeginC.c
	${CC} ${CFLAGS} -g0 -c -o ${.TARGET} ${.ALLSRC}

crtend.o:	crtendC.c
	${CC} ${CFLAGS} -g0 -c -o ${.TARGET} ${.ALLSRC}
.endif

.include <bsd.prog.mk>
