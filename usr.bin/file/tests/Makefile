PACKAGE=	tests
ATF_TESTS_SH=	file_test

SRCDIR=		${SRCTOP}/contrib/file/tests
_files!=	echo ${SRCDIR}/*.testfile ${SRCDIR}/*.result ${SRCDIR}/*.magic
${PACKAGE}FILES+=${_files}

.include <bsd.test.mk>
