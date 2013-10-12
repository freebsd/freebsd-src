# $NetBSD$
# $FreeBSD$
#

.include <bsd.init.mk>

ATF_TESTS:=

.if make(*test)
TESTSDIR?= .
.endif

.if defined(ATF_TESTS_SUBDIRS)
# Only visit subdirs when building, etc because ATF does this it on its own.
.if !make(atf-test)
SUBDIR+= ${ATF_TESTS_SUBDIRS}
.endif
ATF_TESTS+= ${ATF_TESTS_SUBDIRS}

.include <bsd.subdir.mk>
.endif

.if defined(TESTS_C)
ATF_TESTS+= ${TESTS_C}
.for _T in ${TESTS_C}
SRCS.${_T}?= ${_T}.c
DPADD.${_T}+= ${LIBATF_C}
LDADD.${_T}+= -latf-c
.endfor
.endif

.if defined(TESTS_CXX)
ATF_TESTS+= ${TESTS_CXX}
.for _T in ${TESTS_CXX}
SRCS.${_T}?= ${_T}${CXX_SUFFIX:U.cc}
DPADD.${_T}+= ${LIBATF_CXX} ${LIBATF_C}
LDADD.${_T}+= -latf-c++ -latf-c
.endfor
.endif

.if defined(TESTS_SH)
ATF_TESTS+= ${TESTS_SH}
.for _T in ${TESTS_SH}
CLEANFILES+= ${_T} ${_T}.tmp
TESTS_SH_SRC_${_T}?= ${_T}.sh
${_T}: ${TESTS_SH_SRC_${_T}}
	echo '#! /usr/bin/atf-sh' > ${.TARGET}.tmp
	cat ${.ALLSRC} >> ${.TARGET}.tmp
	chmod +x ${.TARGET}.tmp
	mv ${.TARGET}.tmp ${.TARGET}
.endfor
.endif

.include <bsd.test.mk>
