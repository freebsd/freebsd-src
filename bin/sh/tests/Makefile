# $FreeBSD$

.include <bsd.own.mk>

.if ${MACHINE_ARCH} != mips64
TESTS_SUBDIRS+=	builtins
.endif
TESTS_SUBDIRS+=	errors
TESTS_SUBDIRS+=	execution
TESTS_SUBDIRS+=	expansion
TESTS_SUBDIRS+=	parameters
TESTS_SUBDIRS+=	parser
TESTS_SUBDIRS+=	set-e

.include <bsd.test.mk>
