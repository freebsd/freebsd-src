# $FreeBSD$

.include <bsd.own.mk>

TESTSDIR=	${TESTSBASE}/libexec/rtld-elf
SUBDIR+=	libpythagoras target

.PATH:		${.CURDIR:H:H:H:H}/tests

ATF_TESTS_C=	ld_library_pathfds

CFLAGS+=	 -DTESTSDIR="\"${TESTSDIR}\""

.include <bsd.test.mk>
