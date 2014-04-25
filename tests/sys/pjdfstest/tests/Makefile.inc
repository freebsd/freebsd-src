TOP=	${.CURDIR:H:H:H:H:H}

.PATH:	${TOP}/tools/regression/pjdfstest/tests/${.CURDIR:T}
.PATH:	${.CURDIR:H:H:H:H}

TESTSDIR= ${TESTSBASE}/sys/pjdfstest/${.CURDIR:T}

.for _T in ${TAP_TESTS_SH}
TAP_TESTS_SH_SED_${_T}=	-e 's/misc.sh/kyua.sh/g'
TAP_TESTS_SH_SRC_${_T}=	${_T}.t
.endfor
