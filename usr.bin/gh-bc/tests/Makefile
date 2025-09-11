.include <bsd.own.mk>

PACKAGE=		tests

TEST_DIR=		${SRCTOP}/contrib/bc

TESTSDIR=		${TESTSBASE}/usr.bin/gh-bc

.PATH:			${SRCTOP}/tests

FILESGROUPS+=		FILESf
FILESfPACKAGE=		${PACKAGE}
FILESfDIR=		${TESTSDIR}/scripts
FILESf=			${TEST_DIR}/scripts/functions.sh
FILESfMODE=		0755

FILESGROUPS+=		FILEStests
FILEStestsPACKAGE=	${PACKAGE}
FILEStestsDIR=		${TESTSDIR}/tests
FILEStests!=		echo ${TEST_DIR}/tests/*.py ${TEST_DIR}/tests/*.sed \
				${TEST_DIR}/tests/*.sh ${TEST_DIR}/tests/*.txt
FILEStestsMODE=		0755

FILESGROUPS+=		FILESbc
FILESbcPACKAGE=		${PACKAGE}
FILESbcDIR=		${TESTSDIR}/tests/bc
FILESbc!=		echo ${TEST_DIR}/tests/bc/*.*

FILESGROUPS+=		FILESbc_errors
FILESbc_errorsPACKAGE=	${PACKAGE}
FILESbc_errorsDIR=	${TESTSDIR}/tests/bc/errors
FILESbc_errors!=	echo ${TEST_DIR}/tests/bc/errors/*.*

FILESGROUPS+=		FILESbc_scripts
FILESbc_scriptsPACKAGE=	${PACKAGE}
FILESbc_scriptsDIR=	${TESTSDIR}/tests/bc/scripts
FILESbc_scripts!=	echo ${TEST_DIR}/tests/bc/scripts/*.*
FILESbc_scriptsMODE=	0755

FILESGROUPS+=		FILESdc
FILESdcPACKAGE=		${PACKAGE}
FILESdcDIR=		${TESTSDIR}/tests/dc
FILESdc!=		echo ${TEST_DIR}/tests/dc/*.*

FILESGROUPS+=		FILESdc_errors
FILESdc_errorsPACKAGE=	${PACKAGE}
FILESdc_errorsDIR=	${TESTSDIR}/tests/dc/errors
FILESdc_errors!=	echo ${TEST_DIR}/tests/dc/errors/*.*

FILESGROUPS+=		FILESdc_scripts
FILESdc_scriptsPACKAGE=	${PACKAGE}
FILESdc_scriptsDIR=	${TESTSDIR}/tests/dc/scripts
FILESdc_scripts!=	echo ${TEST_DIR}/tests/dc/scripts/*.*
FILESdc_scriptsMODE=	0755

PLAIN_TESTS_SH=		bc_tests dc_tests

bc_tests.sh:
	echo "#!/bin/sh" > ${.TARGET}
	echo 'env LANG=C BC_TEST_OUTPUT_DIR=$$(pwd) \
		${TESTSDIR}/tests/all.sh -n bc 1 1 0 0 bc' >> ${.TARGET}

dc_tests.sh:
	echo "#!/bin/sh" > ${.TARGET}
	echo "env LANG=C BC_TEST_OUTPUT_DIR=\$pwd \
		${TESTSDIR}/tests/all.sh -n dc 1 1 0 0 dc" >> ${.TARGET}

.include <bsd.test.mk>
