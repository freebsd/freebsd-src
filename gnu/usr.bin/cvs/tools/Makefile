#	$Id: Makefile,v 1.3 1997/02/22 15:45:21 peter Exp $

FILES=	INSTALL NEWS README \
	pcl-cvs-lucid.el pcl-cvs-startup.el \
	pcl-cvs.el pcl-cvs.texinfo

NOOBJ=	noobj

EXAMPDIR=	/usr/share/examples/cvs

beforeinstall:
.for file in ${FILES}
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 644 \
		${CVSDIR}/tools/pcl-cvs/${file} ${DESTDIR}${EXAMPDIR}/pcl-cvs
.endfor

.include <bsd.prog.mk>
