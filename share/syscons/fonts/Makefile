UUFONTS=altc-8x16.fnt cp865-8x14.fnt iso-8x16.fnt koi8-8x8.fnt alt-8x14.fnt \
	cp850-8x14.fnt cp865-8x16.fnt iso-8x8.fnt koi8c-8x16.fnt alt-8x16.fnt \
	cp850-8x16.fnt cp865-8x8.fnt koi8-8x14.fnt alt-8x8.fnt cp850-8x8.fnt \
	iso-8x14.fnt koi8-8x16.fnt altb-8x16.fnt koi8b-8x16.fnt

FONTS=altc-8x16 cp865-8x14 iso-8x16 koi8-8x8 alt-8x14 \
	cp850-8x14 cp865-8x16 iso-8x8 koi8c-8x16 alt-8x16 \
	cp850-8x16 cp865-8x8 koi8-8x14 alt-8x8 cp850-8x8 \
	iso-8x14 koi8-8x16 koi8b-8x16 altb-8x16

FONTDIR = /usr/share/syscons/fonts

NOMAN = noman
install:
	cd ${.CURDIR}; \
	install -c -o ${BINOWN} -g ${BINGRP} -m 444 ${UUFONTS} \
		${DESTDIR}${FONTDIR}; \
	install -c -o ${BINOWN} -g ${BINGRP} -m 444 ${FONTS} \
		${DESTDIR}${FONTDIR}

.include <bsd.prog.mk>
