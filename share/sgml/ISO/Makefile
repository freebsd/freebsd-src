#	$FreeBSD$

FILES=	ISOamsa ISOamsb ISOamsc ISOamsn ISOamso ISOamsr
FILES+=	ISObox ISOcyr1 ISOcyr2 ISOdia ISOgrk1 ISOgrk2 ISOgrk3
FILES+=	ISOgrk4 ISOlat1 ISOlat2 ISOnum ISOpub ISOtech
NOOBJ=	noobj

all clean cleandir depend lint tags:

afterinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 ${FILES} \
		${DESTDIR}${BINDIR}/${.CURDIR:T}

.include <bsd.prog.mk> 

