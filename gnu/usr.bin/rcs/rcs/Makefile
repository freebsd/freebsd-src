PROG=	rcs
SRCS=	rcs.c 
CFLAGS+= -I${.CURDIR}/../lib
LDADD=  ${LIBRCS}
DPADD=  ${LIBRCS}

MAN1=	rcs.0 rcsintro.0
MAN5=   rcsfile.0

.include "../../Makefile.inc"
.include <bsd.prog.mk>
