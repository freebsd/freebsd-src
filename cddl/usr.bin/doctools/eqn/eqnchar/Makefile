CLEANFILES=	genutf8 utf-8

FILES+=	ascii eqnchar greek iso utf-8
FILESDIR+=	${PUBDIR}
CFLAGS+=	-DEUC

build-tools: genutf8

genutf8: genutf8.c
	${CC} -o ${.TARGET} ${CFLAGS} ${.IMPSRC}

utf-8:	genutf8
	${.OBJDIR}/genutf8 >${.TARGET}

.include <bsd.prog.mk>

.PATH: ${DOCTOOLSDIR}/eqn/eqnchar.d
