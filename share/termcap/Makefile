#	@(#)Makefile	5.12 (Berkeley) 7/1/90

# reorder gives an editor command for most common terminals
# (in reverse order from n'th to 1'st most commonly used)
# to move them to the front of termcap
#
MAN5=	termcap.5

CLEANFILES+=	termcap

all: termcap ${MAN5}

termcap: reorder termcap.src
	ed - ${.CURDIR}/termcap.src < ${.CURDIR}/reorder > /dev/null

beforeinstall:
	install ${COPY} -o ${BINOWN} -g ${BINGRP} -m 444 termcap \
	     ${DESTDIR}${BINDIR}/misc
	install -c -o ${BINOWN} -g ${BINGRP} -m 444 ${.CURDIR}/map3270 \
	     ${DESTDIR}${BINDIR}/misc
	rm -f ${DESTDIR}/etc/map3270
	ln -s ${BINDIR}/misc/map3270 ${DESTDIR}/etc/map3270
	rm -f ${DESTDIR}/etc/termcap
	ln -s ${BINDIR}/misc/termcap ${DESTDIR}/etc/termcap

.include <bsd.prog.mk>
