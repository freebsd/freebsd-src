
PACKAGE=	tests

TESTSDIR=	${TESTSBASE}/sys/netinet/libalias
BINDIR=		${TESTSDIR}

ATF_TESTS_C+=	1_instance	\
		2_natout	\
		3_natin		\

PROGS+=		perf

LIBADD+=	alias

SRCS.1_instance=1_instance.c util.c
SRCS.2_natout=	2_natout.c util.c
SRCS.3_natin=	3_natin.c util.c
SRCS.perf=	perf.c util.c

.include <bsd.test.mk>

#
# Testing during development
#
test:	all
	cd ${.OBJDIR}; kyua test

report:
	cd ${.OBJDIR}; kyua report

report-v:
	cd ${.OBJDIR}; kyua report --verbose
