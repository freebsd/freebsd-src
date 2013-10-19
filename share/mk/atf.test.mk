# $FreeBSD$
#
# Logic to build and install ATF test programs; i.e. test programs linked
# against the ATF libraries.

.include <bsd.init.mk>

# List of C, C++ and shell test programs to build.
#
# Programs listed here are built using PROGS, PROGS_CXX and SCRIPTS,
# respectively, from bsd.prog.mk.  However, the build rules are tweaked to
# require the ATF libraries.
#
# Test programs registered in this manner are set to be installed into TESTSDIR
# (which should be overriden by the Makefile) and are not required to provide a
# manpage.
ATF_TESTS_C?=
ATF_TESTS_CXX?=
ATF_TESTS_SH?=

.if !empty(ATF_TESTS_C)
PROGS+= ${ATF_TESTS_C}
.for _T in ${ATF_TESTS_C}
BINDIR.${_T}= ${TESTSDIR}
MAN.${_T}?= # empty
SRCS.${_T}?= ${_T}.c
DPADD.${_T}+= ${LIBATF_C}
LDADD.${_T}+= -latf-c
.endfor
.endif

.if !empty(ATF_TESTS_CXX)
PROGS_CXX+= ${ATF_TESTS_CXX}
PROGS+= ${ATF_TESTS_CXX}
.for _T in ${ATF_TESTS_CXX}
BINDIR.${_T}= ${TESTSDIR}
MAN.${_T}?= # empty
SRCS.${_T}?= ${_T}${CXX_SUFFIX:U.cc}
DPADD.${_T}+= ${LIBATF_CXX} ${LIBATF_C}
LDADD.${_T}+= -latf-c++ -latf-c
.endfor
.endif

.if !empty(ATF_TESTS_SH)
SCRIPTS+= ${ATF_TESTS_SH}
.for _T in ${ATF_TESTS_SH}
SCRIPTSDIR_${_T}= ${TESTSDIR}
CLEANFILES+= ${_T} ${_T}.tmp
ATF_TESTS_SH_SRC_${_T}?= ${_T}.sh
${_T}: ${ATF_TESTS_SH_SRC_${_T}}
	echo '#! /usr/bin/atf-sh' > ${.TARGET}.tmp
	cat ${.ALLSRC} >> ${.TARGET}.tmp
	chmod +x ${.TARGET}.tmp
	mv ${.TARGET}.tmp ${.TARGET}
.endfor
.endif

.include <bsd.test.mk>
