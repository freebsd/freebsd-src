# $FreeBSD$

NL=     french
DL=     fr_FR.ISO8859-1
LL=     fr_BE.ISO8859-1 fr_CA.ISO8859-1 fr_CH.ISO8859-1 \
	fr_BE.ISO8859-15 fr_CA.ISO8859-15 fr_CH.ISO8859-15 \
	fr_FR.ISO8859-15

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
