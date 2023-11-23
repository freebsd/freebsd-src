
PROG=	fortune
MAN=	fortune.6
CFLAGS+=-DDEBUG -I${FORTUNE_SRC}/strfile

.include <bsd.prog.mk>

test:	${PROG}
	./${PROG} -m brass
