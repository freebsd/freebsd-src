# $FreeBSD: src/bin/csh/nls/italian/Makefile,v 1.4.2.1 2000/07/04 05:29:28 obrien Exp $

NL=     italian
DL=     it_IT.ISO_8859-1
LL=     it_CH.ISO_8859-1 it_IT.DIS_8859-15 it_CH.DIS_8859-15

.PATH: ${BASESRC}/${NL}

tcsh.msg: set[0-9]*
	cat ${BASESRC}/${NL}/set[0-9] \
	    ${BASESRC}/${NL}/set[0-9][0-9] > ${.TARGET}

install:
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m ${NOBINMODE} \
		tcsh.cat ${DESTDIR}${NLSDIR}/${DL}/tcsh.cat
.for i in ${LL}
	ln -fs ../${DL}/tcsh.cat ${DESTDIR}${NLSDIR}/$i/tcsh.cat
.endfor

.include <bsd.prog.mk>
