PROG=	ed
CFLAGS+=-DVI_BANG -DGNU_REGEX
SRCS=	ed.c re.c buf.c cbc.c 
LINKS=	${BINDIR}/ed ${BINDIR}/red
MLINKS=	ed.1 red.1

.if exists(/usr/lib/libcrypt.a)
CFLAGS+=-DDES
LDADD+= -lcrypt
DPADD+= ${LIBCRYPT}
.endif

LDADD+=	-lgnuregex
DPADD+=	/usr/lib/libgnuregex.a

.include <bsd.prog.mk>
