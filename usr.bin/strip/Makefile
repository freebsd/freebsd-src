#	from: @(#)Makefile	5.4 (Berkeley) 5/11/90
#	$Id: Makefile,v 1.5 1993/09/26 19:27:53 rgrimes Exp $

PROG=	strip

install: maninstall
	install -c -o ${BINOWN} -g ${BINOWN} -m ${BINMODE} strip \
	    ${DESTDIR}${BINDIR}
	./strip ${DESTDIR}${BINDIR}/strip

.include <bsd.prog.mk>
