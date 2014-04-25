SUBDIR=		pjdfstest

TESTS_SUBDIRS+=	tests

TESTSDIR=	${TESTSBASE}/sys/pjdfstest

.PATH: ${.CURDIR:H:H}

test: .PHONY
.if defined(PJDFSTEST_I_KNOW_WHAT_I_AM_DOING)
	cd ${.CURDIR}/tests && make ${.TARGET}
.else
	@echo "Executing the pjdfstest from make test is potentially a really bad idea."
	@echo
	@echo "Call make test again like 'make PJDFSTEST_I_KNOW_WHAT_I_AM_DOING=1 test'"
	@echo "if you want to give this a shot."
.endif

.include <bsd.test.mk>
