# $NetBSD: meta-cmd-cmp.mk,v 1.4 2022/01/27 06:02:59 sjg Exp $
#
# Tests META_MODE command line comparison
#

.MAIN: all

.MAKE.MODE= meta verbose silent=yes curdirok=yes
tf:= .${.PARSEFILE:R}

.if ${.TARGETS:Nall} == ""
all: prep one two change1 change2 filter0 filter1 filter2 filter3 post

CLEANFILES= ${tf}*

prep post: .PHONY
	@rm -f ${CLEANFILES}

.endif

FLAGS?=
FLAGS2?=

tests= ${tf}.cmp ${tf}.nocmp ${tf}.cmp2
filter_tests= ${tf}.filter

${tf}.cmp:
	@echo FLAGS=${FLAGS:Uempty} > $@

${tf}.nocmp: .NOMETA_CMP
	@echo FLAGS=${FLAGS:Uempty} > $@

# a line containing ${.OODATE} will not be compared
# this allows the trick below
${tf}.cmp2:
	@echo FLAGS2=${FLAGS2:Uempty} > $@
	@echo This line not compared FLAGS=${FLAGS:Uempty} ${.OODATE:MNOMETA_CMP}

COMPILER_WRAPPERS+= ccache distcc icecc
WRAPPER?= ccache
.ifdef WITH_CMP_FILTER
.MAKE.META.CMP_FILTER+= ${COMPILER_WRAPPERS:S,^,N,}
.endif
.ifdef WITH_LOCAL_CMP_FILTER
# local variable
${tf}.filter: .MAKE.META.CMP_FILTER= ${COMPILER_WRAPPERS:S,^,N,}
.endif

${tf}.filter:
	@echo ${WRAPPER} cc -c foo.c > $@

# these do the same 
one two: .PHONY
	@echo $@:
	@${.MAKE} -dM -r -C ${.CURDIR} -f ${MAKEFILE} ${tests}

change1: .PHONY
	@echo $@:
	@${.MAKE} -dM -r -C ${.CURDIR} -f ${MAKEFILE} FLAGS=changed ${tests}

change2: .PHONY
	@echo $@:
	@${.MAKE} -dM -r -C ${.CURDIR} -f ${MAKEFILE} FLAGS2=changed ${tests}

filter0: .PHONY
	@echo $@:
	@${.MAKE} -dM -r -C ${.CURDIR} -f ${MAKEFILE} ${filter_tests}

filter1: .PHONY
	@echo $@:
	@${.MAKE} -dM -r -C ${.CURDIR} -f ${MAKEFILE} WRAPPER= ${filter_tests}

filter2: .PHONY
	@echo $@:
	@${.MAKE} -dM -r -C ${.CURDIR} -f ${MAKEFILE} -DWITH_CMP_FILTER \
		WRAPPER=distcc ${filter_tests}

filter3: .PHONY
	@echo $@:
	@${.MAKE} -dM -r -C ${.CURDIR} -f ${MAKEFILE} -DWITH_LOCAL_CMP_FILTER \
		WRAPPER=icecc ${filter_tests}

# don't let gcov mess up the results
.MAKE.META.IGNORE_PATTERNS+= *.gcda
