# $FreeBSD$
#
# The include file <bsd.dep.mk> handles Makefile dependencies.
#
#
# +++ variables +++
#
# DEPENDFILE	dependencies file [.depend]
#
# MKDEP		Options for ${MKDEPCMD} [not set]
#
# MKDEPCMD	Makefile dependency list program [mkdep]
# 
# SRCS          List of source files (c, c++, assembler)
#
#
# +++ targets +++
#
#	cleandepend:
#		Remove depend and tags file
#
#	depend:
#		Make the dependencies for the source files, and store
#		them in the file ${DEPENDFILE}.
#
#	tags:
#		Create a (GLOBAL) gtags file for the source files.
#		If HTML is defined, htags is also run after gtags.


MKDEPCMD?=	CC=${CC} mkdep
DEPENDFILE?=	.depend

.if defined(SRCS)
CLEANFILES?=

.for _LSRC in ${SRCS:M*.l:N*/*}
.for _LC in ${_LSRC:S/.l/.c/}
${_LC}: ${_LSRC}
	${LEX} -t ${LFLAGS} ${.ALLSRC} > ${.TARGET}
SRCS:=	${SRCS:S/${_LSRC}/${_LC}/}
CLEANFILES:= ${CLEANFILES} ${_LC}
.endfor
.endfor

.for _YSRC in ${SRCS:M*.y:N*/*}
.for _YC in ${_YSRC:S/.y/.c/}
SRCS:=	${SRCS:S/${_YSRC}/${_YC}/}
CLEANFILES:= ${CLEANFILES} ${_YC}
.if ${YFLAGS:M-d} != "" && ${SRCS:My.tab.h}
.ORDER: ${_YC} y.tab.h
${_YC} y.tab.h: ${_YSRC}
	${YACC} ${YFLAGS} ${.ALLSRC}
	cp y.tab.c ${_YC}
SRCS:=	${SRCS} y.tab.h
CLEANFILES:= ${CLEANFILES} y.tab.c y.tab.h
.elif ${YFLAGS:M-d} != ""
.for _YH in ${_YC:S/.c/.h/}
.ORDER: ${_YC} ${_YH}
${_YC} ${_YH}: ${_YSRC}
	${YACC} ${YFLAGS} -o ${_YC} ${.ALLSRC}
SRCS:=	${SRCS} ${_YH}
CLEANFILES:= ${CLEANFILES} ${_YH}
.endfor
.else
${_YC}: ${_YSRC}
	${YACC} ${YFLAGS} -o ${_YC} ${.ALLSRC}
.endif
.endfor
.endfor
.endif

.if !target(depend)
.if defined(SRCS)
depend: beforedepend ${DEPENDFILE} afterdepend _SUBDIR

# Different types of sources are compiled with slightly different flags.
# Split up the sources, and filter out headers and non-applicable flags.
${DEPENDFILE}: ${SRCS}
	rm -f ${DEPENDFILE}
.if ${SRCS:M*.[sS]} != ""
	${MKDEPCMD} -f ${DEPENDFILE} -a ${MKDEP} \
	    ${CFLAGS:M-nostdinc*} ${CFLAGS:M-[BID]*} \
	    ${AINC} \
	    ${.ALLSRC:M*.[sS]}
.endif
.if ${SRCS:M*.c} != ""
	${MKDEPCMD} -f ${DEPENDFILE} -a ${MKDEP} \
	    ${CFLAGS:M-nostdinc*} ${CFLAGS:M-[BID]*} \
	    ${.ALLSRC:M*.c}
.endif
.if ${SRCS:M*.cc} != "" || ${SRCS:M*.C} != "" || ${SRCS:M*.cpp} != "" || \
    ${SRCS:M*.cxx} != ""
	${MKDEPCMD} -f ${DEPENDFILE} -a ${MKDEP} \
	    ${CXXFLAGS:M-nostdinc*} ${CXXFLAGS:M-[BID]*} \
	    ${.ALLSRC:M*.cc} ${.ALLSRC:M*.C} ${.ALLSRC:M*.cpp} ${.ALLSRC:M*.cxx}
.endif
.if ${SRCS:M*.m} != ""
	${MKDEPCMD} -f ${DEPENDFILE} -a ${MKDEP} \
	    ${OBJCFLAGS:M-nostdinc*} ${OBJCFLAGS:M-[BID]*} \
	    ${OBJCFLAGS:M-Wno-import*} \
	    ${.ALLSRC:M*.m}
.endif
.if target(_EXTRADEPEND)
	cd ${.CURDIR}; ${MAKE} _EXTRADEPEND
.endif

.ORDER: ${DEPENDFILE} afterdepend
.else
depend: beforedepend afterdepend _SUBDIR
.endif
.if !target(beforedepend)
beforedepend:
.else
.ORDER: beforedepend ${DEPENDFILE}
.ORDER: beforedepend afterdepend
.endif
.if !target(afterdepend)
afterdepend:
.endif
.endif

.if defined(NOTAGS)
tags:
.endif

.if !target(tags)
tags: ${SRCS} _SUBDIR
	@cd ${.CURDIR} && gtags ${GTAGSFLAGS} ${.OBJDIR}
.if defined(HTML)
	@cd ${.CURDIR} && htags ${HTAGSFLAGS} -d ${.OBJDIR} ${.OBJDIR}
.endif
.endif

.if !target(cleandepend)
cleandepend: _SUBDIR
.if defined(SRCS)
	rm -f ${DEPENDFILE} ${.OBJDIR}/GPATH ${.OBJDIR}/GRTAGS \
		${.OBJDIR}/GSYMS ${.OBJDIR}/GTAGS
.if defined(HTML)
	rm -rf ${.OBJDIR}/HTML
.endif
.endif
.endif
