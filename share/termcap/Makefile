#	@(#)Makefile	8.1 (Berkeley) 6/8/93
# $FreeBSD: src/share/termcap/Makefile,v 1.12 1999/08/28 00:22:00 peter Exp $

# reorder gives an editor command for most common terminals
# (in reverse order from n'th to 1'st most commonly used)
# to move them to the front of termcap
#
MAN5=	termcap.5

CLEANFILES+=	termcap termcap.db
FILES=		termcap termcap.db

all: termcap termcap.db

termcap: reorder termcap.src
	ex - ${.CURDIR}/termcap.src < ${.CURDIR}/reorder > /dev/null

termcap.db: termcap
	cap_mkdb termcap

beforeinstall:
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m ${SHAREMODE} \
	     ${FILES} ${DESTDIR}${BINDIR}/misc
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${SHAREMODE} \
	     ${.CURDIR}/map3270 ${DESTDIR}${BINDIR}/misc

etc-termcap:
	rm -f ${DESTDIR}/etc/termcap
	ln -s ${BINDIR}/misc/termcap ${DESTDIR}/etc/termcap

.include <bsd.prog.mk>
