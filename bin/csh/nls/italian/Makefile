# $FreeBSD: src/bin/csh/nls/italian/Makefile,v 1.4.2.2 2002/01/05 15:50:17 phantom Exp $

NL=     italian
DL=     it_IT.ISO8859-1
LL=     it_CH.ISO8859-1 it_IT.ISO8859-15 it_CH.ISO8859-15

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
