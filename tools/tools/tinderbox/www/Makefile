# $FreeBSD$

CGI		 = index.cgi
DATA		 = tb.css
IMAGES		 = daemon.png valid-css.png valid-xhtml10.png
CLEANFILES	+= ${IMAGES}
WWWDIR		?= ${HOME}/public_html
WWWOWN		?= ${USER}
WWWGRP		?= ${USER}

all: ${IMAGES}

realinstall:
	${INSTALL} -m0755 -o${WWWOWN} -g${WWWGRP} ${.CURDIR}/${CGI} ${WWWDIR}
.for FILE in ${DATA} ${IMAGES}
	${INSTALL} -m0644 -o${WWWOWN} -g${WWWGRP} ${.CURDIR}/${FILE} ${WWWDIR}
.endfor

${IMAGES}: ${.TARGET}.uu
	uudecode ${.ALLSRC}

.include <bsd.prog.mk>
