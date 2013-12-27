# $FreeBSD$

.include <bsd.own.mk>

TESTSDIR= ${TESTSBASE}/lib

KYUAFILE= yes

CLEANFILES+= Kyuafile
Kyuafile: ${.CURDIR}/../Kyuafile
	cp -f ${.CURDIR}/../Kyuafile .

.include <bsd.test.mk>
