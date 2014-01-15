# $Id: Makefile 2137 2011-11-10 13:09:30Z jkoshy $

TOP=		../..

.include "${TOP}/mk/elftoolchain.tetvars.mk"

.if !defined(TET_EXECUTE)
TET_EXECUTE=	${.OBJDIR}
.endif

.PHONY:	all clean clobber execute test

all clean depend:

execute test:
	TET_EXECUTE="${TET_EXECUTE}" TET_ROOT="${TET_ROOT}" \
	TET_SUITE_ROOT="${.CURDIR}" \
		${TET_ROOT}/bin/tcc -e .

clobber:	clean
	rm -rf ${TET_RESULTS_DIR} ${TET_TMP_DIR}
