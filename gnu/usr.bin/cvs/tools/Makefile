# $FreeBSD: src/gnu/usr.bin/cvs/tools/Makefile,v 1.7 2000/01/10 11:38:51 phantom Exp $

MAINTAINER=	peter@FreeBSD.org

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
