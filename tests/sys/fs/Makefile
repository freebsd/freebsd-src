# $FreeBSD$

.include <src.opts.mk>
.include <bsd.compiler.mk>

PACKAGE=		tests

TESTSDIR=		${TESTSBASE}/sys/fs

TESTSRC=		${SRCTOP}/contrib/netbsd-tests/fs

#TESTS_SUBDIRS+=	nullfs	# XXX: needs rump
# fusefs tests cannot be compiled/used without the googletest infrastructure.
.if ${COMPILER_FEATURES:Mc++14} && ${MK_GOOGLETEST} != "no"
# Don't build fusefs tests for MIPS while LLVM PR 43263 is active, as part
# of the fusefs tests trigger excessively long compile time.  The build does
# eventually succeed, but it's perhaps better to not do so by default for now.
.if ${MACHINE_CPUARCH} != "mips" || ${COMPILER_TYPE} != "clang"
TESTS_SUBDIRS+=		fusefs
.endif
.endif
TESTS_SUBDIRS+=		tmpfs

${PACKAGE}FILES+=	h_funcs.subr
${PACKAGE}FILESDIR=	${TESTSDIR}

CLEANFILES+=		h_funcs.subr
CLEANFILES+=		h_funcs.subr.tmp

h_funcs.subr: ${TESTSRC}/h_funcs.subr
	cat ${.ALLSRC} | \
	    sed -e '/atf_require_prog mount_$${name}/d' >>${.TARGET}.tmp
	mv ${.TARGET}.tmp ${.TARGET}

.include <bsd.test.mk>
