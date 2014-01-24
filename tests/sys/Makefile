# $FreeBSD$

.include <bsd.own.mk>

SUBDIR= kern
TESTSDIR= ${TESTSBASE}/sys

KYUAFILE= yes

CLEANFILES+= Kyuafile
Kyuafile: ${.CURDIR}/../Kyuafile
	cp -f ${.CURDIR}/../Kyuafile .

.include <bsd.test.mk>
