
FILES=	Termcap Terminfo pcvt.el rc.local uemacs.tar.Z.uu xmodmap-german

beforeinstall:
	for file in ${FILES}; \
	do \
		${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${.CURDIR}/$$file ${DESTDIR}${BINDIR}/Etc/$$file ; \
	done

afterdistribute: beforeinstall

.include "../Makefile.inc"
.include <bsd.subdir.mk>
