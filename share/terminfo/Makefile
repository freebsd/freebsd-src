PACKAGE=	runtime

.PATH: ${SRCTOP}/contrib/ncurses/misc
TINFOBUILDDIR=	${.OBJDIR}/builddir
CLEANDIRS+=	builddir

.include <src.tools.mk>

.if !defined(_SKIP_BUILD)
all: terminfo
.endif
META_TARGETS+=	terminfo install-terminfo

terminfo: terminfo.src
	mkdir -p ${TINFOBUILDDIR}
	${TIC_CMD} -x -o ${TINFOBUILDDIR} ${.ALLSRC}

.if make(*install*)
TINFOS!=	 cd ${TINFOBUILDDIR} && find * -type f | LC_ALL=C sort
TINFOSDIRS=	${TINFOS:C/(.).*/\1/g:O:u}
.endif

beforeinstall: install-terminfo
install-terminfo:
	mkdir -p ${DESTDIR}/usr/share/terminfo
	cd ${DESTDIR}/usr/share/terminfo; mkdir -p ${TINFOSDIRS}
.for f in ${TINFOS}
	${INSTALL} ${TAG_ARGS} \
	    -o ${BINOWN} -g ${BINGRP} -m ${NOBINMODE} \
	    ${TINFOBUILDDIR}/${f} ${DESTDIR}/usr/share/terminfo/${f}
.endfor

.include <bsd.prog.mk>

