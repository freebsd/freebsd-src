# $FreeBSD: src/bin/csh/nls/greek/Makefile,v 1.4.2.1 2000/07/04 05:29:27 obrien Exp $

NL=     greek
DL=     el_GR.ISO_8859-7

.PATH: ${BASESRC}/${NL}

tcsh.msg: set[0-9]*
	cat ${BASESRC}/${NL}/set[0-9] \
	    ${BASESRC}/${NL}/set[0-9][0-9] > ${.TARGET}

install:
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m ${NOBINMODE} \
		tcsh.cat ${DESTDIR}${NLSDIR}/${DL}/tcsh.cat

.include <bsd.prog.mk>
