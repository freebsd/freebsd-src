# $FreeBSD$

PACKAGE=	tests
FILESGROUPS=	TESTS
TESTSPACKAGE=	${PACKAGE}

.PATH:	${.CURDIR}/..

PLAIN_TESTS_C=				option-domain-search_test
SRCS.option-domain-search_test=		alloc.c convert.c hash.c options.c \
					tables.c fake.c option-domain-search.c
CFLAGS.option-domain-search_test+=	-I${.CURDIR}/..
LIBADD.option-domain-search_test=	util

WARNS?=	2

.include <bsd.test.mk>
