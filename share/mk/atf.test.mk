# $FreeBSD$
#
# You must include bsd.test.mk instead of this file from your Makefile.
#
# Logic to build and install ATF test programs; i.e. test programs linked
# against the ATF libraries.

.if !target(__<bsd.test.mk>__)
.error atf.test.mk cannot be included directly.
.endif

# List of C, C++ and shell test programs to build.
#
# Programs listed here are built using PROGS, PROGS_CXX and SCRIPTS,
# respectively, from bsd.prog.mk.  However, the build rules are tweaked to
# require the ATF libraries.
#
# Test programs registered in this manner are set to be installed into TESTSDIR
# (which should be overridden by the Makefile) and are not required to provide a
# manpage.
ATF_TESTS_C?=
ATF_TESTS_CXX?=
ATF_TESTS_SH?=
ATF_TESTS_KSH93?=
ATF_TESTS_PYTEST?=

.if !empty(ATF_TESTS_C)
PROGS+= ${ATF_TESTS_C}
_TESTS+= ${ATF_TESTS_C}
.for _T in ${ATF_TESTS_C}
BINDIR.${_T}= ${TESTSDIR}
MAN.${_T}?= # empty
SRCS.${_T}?= ${_T}.c
DPADD.${_T}+= ${LIBATF_C}
.if empty(LDFLAGS:M-static) && empty(LDFLAGS.${_T}:M-static)
LDADD.${_T}+= ${LDADD_atf_c}
.else
LDADD.${_T}+= ${LIBATF_C}
.endif
TEST_INTERFACE.${_T}= atf
.endfor
.endif

.if !empty(ATF_TESTS_CXX)
PROGS_CXX+= ${ATF_TESTS_CXX}
_TESTS+= ${ATF_TESTS_CXX}
.for _T in ${ATF_TESTS_CXX}
BINDIR.${_T}= ${TESTSDIR}
MAN.${_T}?= # empty
SRCS.${_T}?= ${_T}${CXX_SUFFIX:U.cc}
DPADD.${_T}+= ${LIBATF_CXX} ${LIBATF_C}
.if empty(LDFLAGS:M-static) && empty(LDFLAGS.${_T}:M-static)
LDADD.${_T}+= ${LDADD_atf_cxx} ${LDADD_atf_c}
.else
LDADD.${_T}+= ${LIBATF_CXX} ${LIBATF_C}
.endif
TEST_INTERFACE.${_T}= atf
.endfor
# Silence warnings about usage of deprecated std::auto_ptr
CXXWARNFLAGS+=	-Wno-deprecated-declarations
.endif

.if !empty(ATF_TESTS_SH)
SCRIPTS+= ${ATF_TESTS_SH}
_TESTS+= ${ATF_TESTS_SH}
.for _T in ${ATF_TESTS_SH}
SCRIPTSDIR_${_T}= ${TESTSDIR}
TEST_INTERFACE.${_T}= atf
CLEANFILES+= ${_T} ${_T}.tmp
# TODO(jmmv): It seems to me that this SED and SRC functionality should
# exist in bsd.prog.mk along the support for SCRIPTS.  Move it there if
# this proves to be useful within the tests.
ATF_TESTS_SH_SED_${_T}?= # empty
ATF_TESTS_SH_SRC_${_T}?= ${_T}.sh
${_T}: ${ATF_TESTS_SH_SRC_${_T}}
	echo '#! /usr/libexec/atf-sh' > ${.TARGET}.tmp
.if empty(ATF_TESTS_SH_SED_${_T})
	cat ${.ALLSRC:N*Makefile*} >>${.TARGET}.tmp
.else
	cat ${.ALLSRC:N*Makefile*} \
	    | sed ${ATF_TESTS_SH_SED_${_T}} >>${.TARGET}.tmp
.endif
	chmod +x ${.TARGET}.tmp
	mv ${.TARGET}.tmp ${.TARGET}
.endfor
.endif

.if !empty(ATF_TESTS_KSH93)
SCRIPTS+= ${ATF_TESTS_KSH93}
_TESTS+= ${ATF_TESTS_KSH93}
.for _T in ${ATF_TESTS_KSH93}
SCRIPTSDIR_${_T}= ${TESTSDIR}
TEST_INTERFACE.${_T}= atf
TEST_METADATA.${_T}+= required_programs="ksh93"
CLEANFILES+= ${_T} ${_T}.tmp
# TODO(jmmv): It seems to me that this SED and SRC functionality should
# exist in bsd.prog.mk along the support for SCRIPTS.  Move it there if
# this proves to be useful within the tests.
ATF_TESTS_KSH93_SED_${_T}?= # empty
ATF_TESTS_KSH93_SRC_${_T}?= ${_T}.sh
${_T}: ${ATF_TESTS_KSH93_SRC_${_T}}
	echo '#! /usr/libexec/atf-sh -s/usr/local/bin/ksh93' > ${.TARGET}.tmp
.if empty(ATF_TESTS_KSH93_SED_${_T})
	cat ${.ALLSRC:N*Makefile*} >>${.TARGET}.tmp
.else
	cat ${.ALLSRC:N*Makefile*} \
	    | sed ${ATF_TESTS_KSH93_SED_${_T}} >>${.TARGET}.tmp
.endif
	chmod +x ${.TARGET}.tmp
	mv ${.TARGET}.tmp ${.TARGET}
.endfor
.endif

.if !empty(ATF_TESTS_PYTEST)
# bsd.prog.mk SCRIPTS interface removes file extension unless
# SCRIPTSNAME is set, which is not possible to do here.
# Workaround this by appending another extension (.xtmp) to the
# file name. Use separate loop to avoid dealing with explicitly
# stating expansion for each and every variable.
#
# ATF_TESTS_PYTEST -> contains list of files as is (test_something.py ..)
# _ATF_TESTS_PYTEST -> (test_something.py.xtmp ..)
#
# Former array is iterated to construct Kyuafile, where original file
#  names need to be written.
# Latter array is iterated to enable bsd.prog.mk scripts framework -
#  namely, installing scripts without .xtmp prefix. Note: this allows to
#  not bother about the fact that make target needs to be different from
#  the source file.
_TESTS+= ${ATF_TESTS_PYTEST}
_ATF_TESTS_PYTEST=
.for _T in ${ATF_TESTS_PYTEST}
_ATF_TESTS_PYTEST += ${_T}.xtmp
TEST_INTERFACE.${_T}= atf
TEST_METADATA.${_T}+= required_programs="pytest"
.endfor

SCRIPTS+= ${_ATF_TESTS_PYTEST}
.for _T in ${_ATF_TESTS_PYTEST}
SCRIPTSDIR_${_T}= ${TESTSDIR}
CLEANFILES+= ${_T} ${_T}.tmp
# TODO(jmmv): It seems to me that this SED and SRC functionality should
# exist in bsd.prog.mk along the support for SCRIPTS.  Move it there if
# this proves to be useful within the tests.
ATF_TESTS_PYTEST_SED_${_T}?= # empty
ATF_TESTS_PYTEST_SRC_${_T}?= ${.CURDIR}/${_T:S,.xtmp$,,}
${_T}:
	echo "#! /usr/libexec/atf_pytest_wrapper -P ${TESTSBASE}" > ${.TARGET}.tmp
.if empty(ATF_TESTS_PYTEST_SED_${_T})
	cat ${ATF_TESTS_PYTEST_SRC_${_T}}  >>${.TARGET}.tmp
.else
	cat ${ATF_TESTS_PYTEST_SRC_${_T}} \
	    | sed ${ATF_TESTS_PYTEST_SED_${_T}} >>${.TARGET}.tmp
.endif
	chmod +x ${.TARGET}.tmp
	mv ${.TARGET}.tmp ${.TARGET}
.endfor
.endif
