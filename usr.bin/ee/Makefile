# $FreeBSD$

CFLAGS+= -DCAP -DHAS_NCURSES -DHAS_UNISTD -DHAS_STDARG -DHAS_STDLIB \
	 -DHAS_CTYPE -DHAS_SYS_IOCTL -DHAS_SYS_WAIT -DSLCT_HDR

PROG=	ee
CFLAGS+=-Wall
LINKS=	${BINDIR}/ee ${BINDIR}/ree ${BINDIR}/ee ${BINDIR}/edit
MLINKS=	ee.1 ree.1 ee.1 edit.1
DPADD=	${LIBNCURSES}
LDADD=	-lncurses

LANGS=	en_US.ISO_8859-1 fr_FR.ISO_8859-1 de_DE.ISO_8859-1 pl_PL.ISO_8859-2
FILES=	${LANGS:S/$/.ee.cat/}
CLEANFILES+=	${FILES}

all: ${FILES}

.for lang in ${LANGS}
${lang}.ee.cat: ${.CURDIR}/nls/${lang}/ee.msg
	gencat -new ${.TARGET} ${.ALLSRC}
.endfor

beforeinstall:
.for lang in ${LANGS}
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m ${NOBINMODE} \
	    ${lang}.ee.cat ${DESTDIR}${NLSDIR}/${lang}/ee.cat
.endfor

.include <bsd.prog.mk>
