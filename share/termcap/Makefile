#	@(#)Makefile	8.1 (Berkeley) 6/8/93

# reorder gives an editor command for most common terminals
# (in reverse order from n'th to 1'st most commonly used)
# to move them to the front of termcap
#
MAN5=	termcap.0

all: termcap termcap.db ${MAN5}

termcap: reorder termcap.src
	ex - ${.CURDIR}/termcap.src < ${.CURDIR}/reorder > /dev/null

termcap.db: termcap
	cap_mkdb termcap

clean:
	rm -f termcap termcap.db

cleandir: clean
	rm -f ${MAN5}

beforeinstall:
	install -o ${BINOWN} -g ${BINGRP} -m 444 termcap \
	     ${DESTDIR}${BINDIR}/misc
	install -o ${BINOWN} -g ${BINGRP} -m 444 termcap.db \
	     ${DESTDIR}${BINDIR}/misc
	install -c -o ${BINOWN} -g ${BINGRP} -m 444 ${.CURDIR}/map3270 \
	     ${DESTDIR}${BINDIR}/misc

	
.include <bsd.prog.mk>
