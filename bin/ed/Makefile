PROG=	ed
CFLAGS+=-I${.CURDIR} -DVI_BANG -DGNU_REGEX -DHAVE_STRING_H=1
SRCS=	ed.c re.c buf.c cbc.c regex.c
LINKS=	${BINDIR}/ed ${BINDIR}/red
MLINKS=	ed.1 red.1

.if exists(/usr/lib/libcrypt.a)
CFLAGS+=-DDES
DPADD+= ${LIBCRYPT}
LDADD+= -lcrypt
.endif


.include <bsd.prog.mk>
