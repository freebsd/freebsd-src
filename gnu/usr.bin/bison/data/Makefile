# $FreeBSD$

BISONDIR=${.CURDIR}/../../../../contrib/bison

.PATH:	${BISONDIR}

DATADIR= ${SHAREDIR}/misc
CLEANFILES+= bison.s1

all:	bison.s1

# Copy bison.simple, inserting directory name into the #line commands.
bison.s1: bison.simple
	sed -e "/^#line/ s|bison|${DATADIR}/bison|" \
		< ${BISONDIR}/bison.simple > bison.s1

install:
	${INSTALL} ${COPY} -o bin -g bin -m 444 bison.s1 \
		${DESTDIR}${DATADIR}/bison.simple
	${INSTALL} -c -o bin -g bin -m 444 ${BISONDIR}/bison.hairy \
		${DESTDIR}${DATADIR}

.include <bsd.prog.mk>
