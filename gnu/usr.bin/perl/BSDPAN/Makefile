# $FreeBSD$
#
# Doing a make install builds /usr/libdata/perl/BSDPAN

DDIR=	${DESTDIR}/usr/libdata/perl/BSDPAN

NOOBJ=	noobj

all clean cleandir depend lint tags:

FILES=	BSDPAN.pm BSDPAN/Override.pm ExtUtils/MM_Unix.pm ExtUtils/Packlist.pm

.for file in ${FILES}
beforeinstall::
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 644 ${file} ${DDIR}/${file}
.endfor

.include <bsd.prog.mk>
