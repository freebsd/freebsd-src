CFLAGS+= -DCAP -DHAS_NCURSES -DHAS_UNISTD -DHAS_STDARG -DHAS_STDLIB \
	 -DHAS_CTYPE -DHAS_SYS_IOCTL -DHAS_SYS_WAIT -DSLCT_HDR

PROG=	ee
SRCS=	ee.c
LINKS=	${BINDIR}/ee ${BINDIR}/ree
MLINKS=        ee.1 ree.1
DPADD=	${LIBNCURSES} ${LIBMYTINFO}
LDADD=	-lncurses -lmytinfo

LANGS=	en_US.ISO_8859-1 fr_FR.ISO_8859-1 de_DE.ISO_8859-1

afterinstall:
.for l in ${LANGS}
	@${ECHO} "Install ${DESTDIR}${NLSDIR}/${l}/ee.cat"
	@rm -f ${DESTDIR}${NLSDIR}/${l}/ee.cat
	@gencat -new ${DESTDIR}${NLSDIR}/${l}/ee.cat ${.CURDIR}/nls/${l}/ee.msg
	@chown ${BINOWN}.${BINGRP} ${DESTDIR}${NLSDIR}/${l}/ee.cat
	@chmod 444 ${DESTDIR}${NLSDIR}/${l}/ee.cat
.endfor


.include <bsd.prog.mk>
