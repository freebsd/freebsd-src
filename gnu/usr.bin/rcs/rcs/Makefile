PROG= rcs

SRCS=	rcs.c 
LDADD=	-L${.CURDIR}/../lib/${.CURDIR} -lrcs
CFLAGS+= -I${.CURDIR}/../lib

MAN1=	rcs.0 rcsintro.0
MAN5=   rcsfile.0

.include <bsd.prog.mk>
