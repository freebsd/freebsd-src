#
# $FreeBSD$
#
SRCS=	crt_sb.S				\
	crtbeginC.c				\
	crtendC.c
FILES=	crt_sb.o				\
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
