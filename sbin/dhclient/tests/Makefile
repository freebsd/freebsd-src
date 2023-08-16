
.PATH:	${.CURDIR:H}

ATF_TESTS_SH=	pcp

# Tests assign a common IP address.
TEST_METADATA.pcp+=	is_exclusive=true

PLAIN_TESTS_C=				option-domain-search_test
SRCS.option-domain-search_test=		alloc.c convert.c hash.c options.c \
					tables.c parse.c conflex.c tree.c fake.c \
					option-domain-search.c
CFLAGS.option-domain-search_test+=	-I${.CURDIR:H}
LIBADD.option-domain-search_test=	util

WARNS?=	2

.include <bsd.test.mk>
