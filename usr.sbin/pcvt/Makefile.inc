BINDIR	=	/usr/local/bin
FONTDIR	=	/usr/share/misc/pcvtfonts
LIBDIR	=	/usr/local/lib
LIBMODE	=	644

# for pcvt_ioctl.h

INCDIR	=	/usr/include/machine
INCOWN	=	bin
INCGRP	=	bin
INCMODE	=	444

${INCDIR}/pcvt_ioctl.h: ${.CURDIR}/../../pcvt_ioctl.h
	install ${COPY} -o ${INCOWN} -g ${INCGRP} -m ${INCMODE} \
	${.CURDIR}/../../pcvt_ioctl.h ${INCDIR}

.depend:	${INCDIR}/pcvt_ioctl.h

# for FreeBSD (they nroffify "on the fly")

MAN3EXT	=	3
MAN4EXT	=	4
MAN5EXT	=	5
MAN8EXT	=	8
MANDIR	=	/usr/local/man/man
