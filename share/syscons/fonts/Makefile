FONTS =  cp850-8x14 cp850-8x16 cp850-8x8 cp865-8x14 cp865-8x16 \
		cp865-8x8 iso-8x14 iso-8x16 iso-8x8
FONTDIR = /usr/share/syscons/fonts

install:
	cd ${.CURDIR}; \
	install -c -o ${BINOWN} -g ${BINGRP} -m 444 ${FONTS} \
		${DESTDIR}${FONTDIR}

.include <bsd.prog.mk>
