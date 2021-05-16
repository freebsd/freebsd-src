# $FreeBSD$

.include <bsd.own.mk>

PACKAGE=	tests

TESTSDIR=	${TESTSBASE}/sys/netinet/libalias
BINDIR=		${TESTSDIR}

ATF_TESTS_C+=	1_instance

LIBADD+=	alias

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
