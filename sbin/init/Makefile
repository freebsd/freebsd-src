
PROG=	init
#SRCS=	init.c 
DPADD=	${LIBUTIL}
LDADD=	-lutil
NOMAN=noman
LDFLAGS=

beforeinstall:
	rm -f ${DESTDIR}${BINDIR}/init.old
	- ln ${DESTDIR}${BINDIR}/init ${DESTDIR}${BINDIR}/init.old

.include <bsd.prog.mk>
