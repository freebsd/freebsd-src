# $FreeBSD: src/gnu/usr.bin/cvs/tools/Makefile,v 1.4.2.2 1999/12/13 20:57:31 peter Exp $

MAINAINER=	peter@FreeBSD.org

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
