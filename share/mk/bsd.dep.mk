# $FreeBSD$
#
# The include file <bsd.dep.mk> handles Makefile dependencies.
#
#
# +++ variables +++
#
# CTAGS		A tags file generation program [gtags]
#
# CTAGSFLAGS	Options for ctags(1) [not set]
#
# DEPENDFILE	dependencies file [.depend]
#
# GTAGSFLAGS	Options for gtags(1) [-o]
#
# HTAGSFLAGS	Options for htags(1) [not set]
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
#		In "ctags" mode, create a tags file for the source files.
#		In "gtags" mode, create a (GLOBAL) gtags file for the
#		source files.  If HTML is defined, htags(1) is also run
#		after gtags(1).

.if !target(__<bsd.init.mk>__)
.error bsd.dep.mk cannot be included directly.
.endif

CTAGS?=		gtags
CTAGSFLAGS?=
GTAGSFLAGS?=	-o
HTAGSFLAGS?=

.if ${CC} != "cc"
MKDEPCMD?=	CC='${CC}' mkdep
.else
MKDEPCMD?=	mkdep
.endif
DEPENDFILE?=	.depend

# Keep `tags' here, before SRCS are mangled below for `depend'.
.if !target(tags) && defined(SRCS) && !defined(NOTAGS)
tags: ${SRCS}
.if ${CTAGS:T} == "ctags"
	@${CTAGS} ${CTAGSFLAGS} -f /dev/stdout \
	    ${.ALLSRC:N*.h} | sed "s;${.CURDIR}/;;" > ${.TARGET}
.elif ${CTAGS:T} == "gtags"
	@cd ${.CURDIR} && ${CTAGS} ${GTAGSFLAGS} ${.OBJDIR}
.if defined(HTML)
	@cd ${.CURDIR} && htags ${HTAGSFLAGS} -d ${.OBJDIR} ${.OBJDIR}
.endif
.endif
.endif

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
depend: beforedepend ${DEPENDFILE} afterdepend

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
_EXTRADEPEND: .USE
${DEPENDFILE}: _EXTRADEPEND
.endif

.ORDER: ${DEPENDFILE} afterdepend
.else
depend: beforedepend afterdepend
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

.if !target(cleandepend)
cleandepend:
.if defined(SRCS)
.if ${CTAGS:T} == "ctags"
	rm -f ${DEPENDFILE} tags
.elif ${CTAGS:T} == "gtags"
	rm -f ${DEPENDFILE} GPATH GRTAGS GSYMS GTAGS
.if defined(HTML)
	rm -rf HTML
.endif
.endif
.endif
.endif

.if !target(checkdpadd) && (defined(DPADD) || defined(LDADD))
checkdpadd:
	@ldadd=`echo \`for lib in ${DPADD} ; do \
		echo $$lib | sed 's;^/usr/lib/lib\(.*\)\.a;-l\1;' ; \
	done \`` ; \
	ldadd1=`echo ${LDADD}` ; \
	if [ "$$ldadd" != "$$ldadd1" ] ; then \
		echo ${.CURDIR} ; \
		echo "DPADD -> $$ldadd" ; \
		echo "LDADD -> $$ldadd1" ; \
	fi
.endif
