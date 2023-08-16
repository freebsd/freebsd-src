
.include <src.opts.mk>
.include <bsd.compiler.mk>

PACKAGE=		tests

TESTSDIR=		${TESTSBASE}/sys/fs

TESTSRC=		${SRCTOP}/contrib/netbsd-tests/fs

#TESTS_SUBDIRS+=	nullfs	# XXX: needs rump
# fusefs tests cannot be compiled/used without the googletest infrastructure.
.if ${COMPILER_FEATURES:Mc++14} && ${MK_GOOGLETEST} != "no"
TESTS_SUBDIRS+=		fusefs
.endif
TESTS_SUBDIRS+=		tarfs
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
