#	@(#)Makefile	8.2 (Berkeley) 3/27/94

MELIB=	${BINDIR}/me
TMLIB=	${BINDIR}/tmac
MESRCS=	acm.me chars.me deltext.me eqn.me float.me footnote.me \
	index.me letterhead.me local.me null.me refer.me sh.me \
	tbl.me thesis.me
TMSRCS=	tmac.orig_me
FILES=	${MESRCS} ${TMSRCS}
CLEANFILES+=	${FILES:S/$/.tmp/}

# XXX use groff version
# MAN7=	me.7

all: ${FILES:S/$/.tmp/}

.for file in ${FILES}
${file}.tmp: ${file}
	sed -f ${.CURDIR}/strip.sed < ${.ALLSRC} > ${.TARGET}
.endfor

beforeinstall:
.for file in ${MESRCS}
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m ${NOBINMODE} \
	    ${file}.tmp ${DESTDIR}${MELIB}/${file}
.endfor
.for file in ${TMSRCS}
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m ${NOBINMODE} \
	    ${file}.tmp ${DESTDIR}${TMLIB}/${file}
.endfor
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${NOBINMODE} \
	    ${.CURDIR}/revisions ${DESTDIR}${MELIB}/

.include <bsd.prog.mk>
