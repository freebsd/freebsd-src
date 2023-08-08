# $FreeBSD$

PACKAGE=	tests

TESTSDIR=	${TESTSBASE}/sys/vm

ATF_TESTS_C+=	mlock_test \
		mmap_test \
		page_fault_signal \
		shared_shadow_inval_test

.if ${MACHINE_ARCH} != "i386" && ${MACHINE} != "arm" && \
	(${MACHINE} != "powerpc" || ${MACHINE_ARCH} != "powerpc")
	# MAP_32BIT is only available on 64-bit platforms
BINDIR=		${TESTSDIR}
ATF_TESTS_SH+=	mmap_map_32bit_test
PROGS+=		mmap_map_32bit_helper
.endif

SUBDIR=		soxstack
TESTS_SUBDIRS+=	stack

SUBDIR_DEPEND_stack=soxstack

.include <bsd.test.mk>
