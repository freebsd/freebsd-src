# $FreeBSD: src/bin/csh/nls/ukrainian/Makefile,v 1.1.2.1 2001/03/05 05:47:06 obrien Exp $

NL=     ukrainian
DL=     uk_UA.KOI8-U

.PATH: ${BASESRC}/${NL}

tcsh.msg: set[0-9]*
	cat ${BASESRC}/${NL}/set[0-9] \
	    ${BASESRC}/${NL}/set[0-9][0-9] > ${.TARGET}

install:
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m ${NOBINMODE} \
		tcsh.cat ${DESTDIR}${NLSDIR}/${DL}/tcsh.cat

.include <bsd.prog.mk>
