# $FreeBSD$

CFLAGS+= -DCAP -DHAS_NCURSES -DHAS_UNISTD -DHAS_STDARG -DHAS_STDLIB \
	 -DHAS_CTYPE -DHAS_SYS_IOCTL -DHAS_SYS_WAIT -DSLCT_HDR

PROG=	ee
CFLAGS+=-Wall
LINKS=	${BINDIR}/ee ${BINDIR}/ree ${BINDIR}/ee ${BINDIR}/edit
MLINKS=	ee.1 ree.1 ee.1 edit.1
DPADD=	${LIBNCURSES}
LDADD=	-lncurses

LANGS=	en_US.ISO_8859-1 fr_FR.ISO_8859-1 de_DE.ISO_8859-1 pl_PL.ISO_8859-2 \
	uk_UA.KOI8-U ru_RU.KOI8-R
8859_15_LINKS = fr_FR de_DE

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
.for link in ${8859_15_LINKS}
	ln -sf ../${link}.ISO8859-1/ee.cat \
		${DESTDIR}${NLSDIR}/${link}.ISO8859-15/ee.cat
.endfor

.include <bsd.prog.mk>
