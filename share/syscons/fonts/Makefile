# $FreeBSD$

FONTS=	cp437-8x8.fnt cp437-8x14.fnt cp437-8x16.fnt \
	cp437-thin-8x8.fnt cp437-thin-8x16.fnt \
	cp850-8x8.fnt cp850-8x14.fnt cp850-8x16.fnt \
	cp850-thin-8x8.fnt cp850-thin-8x16.fnt \
	cp865-8x8.fnt cp865-8x14.fnt cp865-8x16.fnt \
	cp865-thin-8x8.fnt cp865-thin-8x16.fnt \
	cp866-8x8.fnt cp866-8x14.fnt cp866-8x16.fnt \
	cp866b-8x16.fnt cp866c-8x16.fnt \
	iso-8x8.fnt iso-8x14.fnt iso-8x16.fnt \
	iso-thin-8x16.fnt \
	iso-8859-2-8x16.fnt \
	koi8-r-8x8.fnt koi8-r-8x14.fnt koi8-r-8x16.fnt \
	koi8-rb-8x16.fnt koi8-rc-8x16.fnt \
	INDEX.fonts

FONTDIR = /usr/share/syscons/fonts

NOMAN = noman
NOOBJ = noobj

install:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 ${FONTS} \
		${DESTDIR}${FONTDIR}

.include <bsd.prog.mk>
