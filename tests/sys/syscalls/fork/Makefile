.include <bsd.compiler.mk>

TESTSDIR=	${TESTSBASE}/sys/syscalls/fork

ATF_TESTS_C=	fork_test

.if ${COMPILER_TYPE} == "gcc"
CFLAGS.bitstring_test=	-fno-strict-overflow
.endif

.include <bsd.test.mk>
