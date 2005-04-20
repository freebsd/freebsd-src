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
.for _data in ${DATA}
	${INSTALL} -m0644 -o${WWWOWN} -g${WWWGRP} ${.CURDIR}/${_data} ${WWWDIR}
.endfor
.for _image in ${IMAGES}
	${INSTALL} -m0644 -o${WWWOWN} -g${WWWGRP} ${_image} ${WWWDIR}
.endfor

${IMAGES}: ${.TARGET}.uu
	uudecode ${.ALLSRC}

.include <bsd.prog.mk>
