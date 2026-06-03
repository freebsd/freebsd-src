PACKAGE=	tests
ATF_TESTS_SH=	file_test

SRCDIR=		${SRCTOP}/contrib/file/tests
_files!=	echo ${SRCDIR}/*.testfile ${SRCDIR}/*.flags ${SRCDIR}/*.magic
_results!=	echo ${SRCDIR}/*.result
${PACKAGE}FILES+=${_files}

# Generate normalized .result files in the obj directory, ensuring each ends
# with a newline (some upstream result files are missing one).  Install those
# instead of the originals from contrib.
.for _r in ${_results}
${_r:T}: ${_r}
	awk 1 ${.ALLSRC} > ${.TARGET}
${PACKAGE}FILES+=	${_r:T}
CLEANFILES+=	${_r:T}
.endfor

.include <bsd.test.mk>
