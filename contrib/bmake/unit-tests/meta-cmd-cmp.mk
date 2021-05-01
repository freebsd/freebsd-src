# $NetBSD: meta-cmd-cmp.mk,v 1.2 2020/12/05 22:51:34 sjg Exp $
#
# Tests META_MODE command line comparison
#

.MAIN: all

.MAKE.MODE= meta verbose silent=yes curdirok=yes
tf:= .${.PARSEFILE:R}

.if ${.TARGETS:Nall} == ""
all: prep one two change1 change2 post

CLEANFILES= ${tf}*

prep post: .PHONY
	@rm -f ${CLEANFILES}

.endif

FLAGS?=
FLAGS2?=

tests= ${tf}.cmp ${tf}.nocmp ${tf}.cmp2

${tf}.cmp:
	@echo FLAGS=${FLAGS:Uempty} > $@

${tf}.nocmp: .NOMETA_CMP
	@echo FLAGS=${FLAGS:Uempty} > $@

# a line containing ${.OODATE} will not be compared
# this allows the trick below
${tf}.cmp2:
	@echo FLAGS2=${FLAGS2:Uempty} > $@
	@echo This line not compared FLAGS=${FLAGS:Uempty} ${.OODATE:MNOMETA_CMP}

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

# don't let gcov mess up the results
.MAKE.META.IGNORE_PATTERNS+= *.gcda
