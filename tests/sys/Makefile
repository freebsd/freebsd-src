# $FreeBSD$

.include <bsd.own.mk>

TESTSDIR= ${TESTSBASE}/sys

KYUAFILE= yes

CLEANFILES+= Kyuafile
Kyuafile: ${.CURDIR}/../Kyuafile
	cp -f ${.CURDIR}/../Kyuafile .

.include <bsd.test.mk>
