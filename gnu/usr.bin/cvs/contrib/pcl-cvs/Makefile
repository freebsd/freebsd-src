#	$Id: Makefile,v 1.2 1995/12/11 00:45:41 peter Exp $

FILES=	ChangeLog INSTALL NEWS README \
	compile-all.el pcl-cvs-lucid.el pcl-cvs-startup.el \
	pcl-cvs.el pcl-cvs.texinfo compile.sh

NOOBJ=	noobj

EXAMPDIR=	/usr/share/examples/cvs

afterinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 644 \
		${PCLCVS} ${DESTDIR}${EXAMPDIR}/pcl-cvs

.include "../../../Makefile.inc"
.include <bsd.prog.mk>
