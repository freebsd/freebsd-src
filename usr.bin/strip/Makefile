#	@(#)Makefile	8.1 (Berkeley) 6/6/93

PROG=	strip

install:
	install -c -o ${BINOWN} -g ${BINOWN} -m ${BINMODE} strip \
	    ${DESTDIR}${BINDIR}
	./strip ${DESTDIR}${BINDIR}/strip

.include <bsd.prog.mk>
