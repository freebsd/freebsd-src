CFLAGS+= -DCAP -DHAS_NCURSES -DHAS_UNISTD -DHAS_STDARG -DHAS_STDLIB \
	 -DHAS_CTYPE -DHAS_SYS_IOCTL -DHAS_SYS_WAIT -DSLCT_HDR

PROG=	ee
SRCS=	ee.c
LINKS=	${BINDIR}/ee ${BINDIR}/ree
DPADD=	${LIBNCURSES} ${LIBMYTINFO}
LDADD=	-lncurses -lmytinfo

LANGS=	en_US.ISO_8859-1 fr_FR.ISO_8859-1 de_DE.ISO_8859-1
NLSDIR=	${DESTDIR}/usr/share/nls

afterinstall:
	for l in ${LANGS}; do \
		gencat -new ${NLSDIR}/$$l/ee.cat ${.CURDIR}/nls/$$l/ee.msg; \
		chown ${BINOWN}.${BINGRP} ${NLSDIR}/$$l/ee.cat; \
		chmod 444 ${NLSDIR}/$$l/ee.cat; \
	done

.include <bsd.prog.mk>
