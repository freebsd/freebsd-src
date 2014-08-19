#
# $FreeBSD$
#
SRCS=	crt_sb.S
FILES=	crt_sb.o
FILESOWN=       ${LIBOWN}
FILESGRP=       ${LIBGRP}
FILESMODE=      ${LIBMODE}
FILESDIR=       ${LIBDIR}
CLEANFILES=	${FILES}

USE_CHERI=	yes
USE_CHERI_STACK=	yes

.include <src.opts.mk>
.include <bsd.prog.mk>
