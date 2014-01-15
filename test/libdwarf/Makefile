#
# $Id: Makefile 2137 2011-11-10 13:09:30Z jkoshy $
#

TOP=	../..

.include "${TOP}/mk/elftoolchain.tetvars.mk"

.if make(tccbuild)
TET_OPTIONS+=	-b
.endif

.if make(tccclean)
TET_OPTIONS+=	-c
.endif

.if make(execute) || make(test)
TET_OPTIONS+=	-e
.endif

.MAIN: all

.PHONY:	clobber execute tccbuild tccclean test

execute tccbuild tccclean test:
	TET_ROOT=${TET_ROOT} TET_SUITE_ROOT=${.CURDIR} \
	${TET_ROOT}/bin/tcc ${TET_OPTIONS} .

clobber:	clean
	rm -rf ${TET_RESULTS_DIR} ${TET_TMP_DIR}

SUBDIR=		ts

.include <bsd.subdir.mk>
