PROG=	rcs
SRCS=	rcs.c 
CFLAGS+= -I${.CURDIR}/../lib
LDADD=  ${LIBRCS}
DPADD=  ${LIBRCS}

MAN1=	rcs.1 rcsintro.1
MAN5=   rcsfile.5

.include "../../Makefile.inc"
.include <bsd.prog.mk>
