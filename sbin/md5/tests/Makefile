# $FreeBSD$

.include <bsd.own.mk>

PACKAGE=		tests

TEST_DIR=		${SRCTOP}/sbin/md5/tests

.PATH:			${TEST_DIR}

FILESGROUPS+=		FILESinputs
FILESinputsPACKAGE=	${PACKAGE}
FILESinputsDIR=		${TESTSDIR}
FILESinputs!=		echo ${TEST_DIR}/*.inp ${TEST_DIR}/*.in

FILESGROUPS+=		FILESchkfiles
FILESchkfilesPACKAGE=	${PACKAGE}
FILESchkfilesDIR=	${TESTSDIR}
FILESchkfiles!=		echo ${TEST_DIR}/*.chk

FILESGROUPS+=		FILESdigests
FILESdigestsPACKAGE=	${PACKAGE}
FILESdigestsDIR=	${TESTSDIR}
FILESdigests!=		echo ${TEST_DIR}/*.digest

FILESGROUPS+=		FILESparam
FILESparamPACKAGE=	${PACKAGE}
FILESparamDIR=		${TESTSDIR}
FILESparam!=		echo ${TEST_DIR}/*.txt

ATF_TESTS_SH+=		md5_test

PLAIN_TESTS_SH+=	self-test
PLAIN_TESTS_SH+=	bsd-c-test
PLAIN_TESTS_SH+=	bsd-p-test
PLAIN_TESTS_SH+=	bsd-s-test
PLAIN_TESTS_SH+=	coreutils-c-test

.SUFFIXES:		.SH

.SH.sh:
	sed 's|%%TESTSBASE%%|${TESTSBASE}|g' ${.ALLSRC} > ${.TARGET}

.include <bsd.test.mk>
