#	@(#)Makefile	8.1 (Berkeley) 6/11/93

SUBDIR=	boggle mkdict mkindex

.if exists(${.CURDIR}/mkdict/obj)
MKDICT=	${.CURDIR}/mkdict/obj/mkdict
.else
MKDICT=	${.CURDIR}/mkdict/mkdict
.endif
.if exists(${.CURDIR}/mkindex/obj)
MKINDX=	${.CURDIR}/mkindex/obj/mkindex
.else
MKINDX=	${.CURDIR}/mkindex/mkindex
.endif

afterdistribute afterinstall:
	${MKDICT} < ${.CURDIR}/../../share/dict/web2 > \
	    ${DESTDIR}/usr/share/games/boggle/dictionary
	${MKINDX} < ${DESTDIR}/usr/share/games/boggle/dictionary > \
	    ${DESTDIR}/usr/share/games/boggle/dictindex
	chown ${BINOWN}.${BINGRP} ${DESTDIR}/usr/share/games/boggle/dictionary
	chmod 444 ${DESTDIR}/usr/share/games/boggle/dictionary
	chown ${BINOWN}.${BINGRP} ${DESTDIR}/usr/share/games/boggle/dictindex
	chmod 444 ${DESTDIR}/usr/share/games/boggle/dictindex

.include <bsd.subdir.mk>
