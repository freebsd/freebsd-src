# $FreeBSD$

CFLAGS+= -DCAP -DHAS_NCURSES -DHAS_UNISTD -DHAS_STDARG -DHAS_STDLIB \
	 -DHAS_CTYPE -DHAS_SYS_IOCTL -DHAS_SYS_WAIT -DSLCT_HDR

PROG=	ee
CFLAGS+=-Wall
LINKS=	${BINDIR}/ee ${BINDIR}/ree ${BINDIR}/ee ${BINDIR}/edit
MLINKS=	ee.1 ree.1 ee.1 edit.1
DPADD=	${LIBNCURSES}
LDADD=	-lncurses

LANGS=	en_US.US-ASCII fr_FR.ISO8859-1 de_DE.ISO8859-1 pl_PL.ISO8859-2 \
	uk_UA.KOI8-U ru_RU.KOI8-R
8859_15_LINKS = fr_FR de_DE
ENUS_LINKS =	ISO8859-1 ISO8859-15

FILES=	${LANGS:S/$/.ee.cat/}
CLEANFILES+=	${FILES}

.for lang in ${LANGS}
${lang}.ee.cat: ${.CURDIR}/nls/${lang}/ee.msg
	gencat -new ${.TARGET} ${.ALLSRC}
.endfor

.for lang in ${LANGS}
FILESDIR_${lang:S/$/.ee.cat/}=	${NLSDIR}/${lang}
FILESNAME_${lang:S/$/.ee.cat/}=	ee.cat
.endfor

.for link in ${ENUS_LINKS}
SYMLINKS+=	../en_US.US-ASCII/ee.cat ${NLSDIR}/en_US.${link}/ee.cat
.endfor
.for link in ${8859_15_LINKS}
SYMLINKS+=	../${link}.ISO8859-1/ee.cat ${NLSDIR}/${link}.ISO8859-15/ee.cat
.endfor

.include <bsd.prog.mk>
