# $FreeBSD: src/bin/csh/nls/french/Makefile,v 1.4.2.1 2000/07/04 05:29:27 obrien Exp $

NL=     french
DL=     fr_FR.ISO_8859-1
LL=     fr_BE.ISO_8859-1 fr_CA.ISO_8859-1 fr_CH.ISO_8859-1 \
	fr_BE.DIS_8859-15 fr_CA.DIS_8859-15 fr_CH.DIS_8859-15 \
	fr_FR.DIS_8859-15

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
