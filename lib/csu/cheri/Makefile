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

USE_CHERI=yes

.include <src.opts.mk>
.include <bsd.prog.mk>
