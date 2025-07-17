TESTSDIR?=	${TESTSBASE}/libexec/rtld-elf/rtld_deepbind
ATF_TESTS_C=	rtld_deepbind

LIBVAL=	${.OBJDIR}/../libval
LDFLAGS.rtld_deepbind+=	-L${LIBVAL} -Wl,-rpath,'$$ORIGIN'
DPADD+=			-lval
LDADD+=			-lval

.include <bsd.test.mk>
