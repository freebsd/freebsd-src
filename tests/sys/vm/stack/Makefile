
PACKAGE=	tests

TESTSDIR=	${TESTSBASE}/sys/vm/stack

ATF_TESTS_C+=	stack_dt_need_exec_test
ATF_TESTS_C+=	stack_dlopen_exec_test
ATF_TESTS_C+=	stack_mprotect_exec_test

LDFLAGS.stack_dt_need_exec_test+= -Wl,-rpath,${TESTSDIR} -L${.OBJDIR:H}/soxstack
LDADD.stack_dt_need_exec_test+= -lsoxstack
LDFLAGS.stack_dlopen_exec_test+= -Wl,-rpath,${TESTSDIR}

.include <bsd.test.mk>
