# $FreeBSD: src/bin/csh/nls/ukrainian/Makefile,v 1.1.2.2 2002/08/07 16:31:39 ru Exp $

NL=     ukrainian
DL=     uk_UA.KOI8-U

.PATH: ${BASESRC}/${NL}

tcsh.msg: set[0-9]*
	cat ${BASESRC}/${NL}/set[0-9] \
	    ${BASESRC}/${NL}/set[0-9][0-9] > ${.TARGET}

install:
	${INSTALL} -o ${BINOWN} -g ${BINGRP} -m ${NOBINMODE} \
		tcsh.cat ${DESTDIR}${NLSDIR}/${DL}/tcsh.cat

.include <bsd.prog.mk>
