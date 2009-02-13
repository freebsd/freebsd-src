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
# DPSRCS	List of source files which are needed for generating
#		dependencies, ${SRCS} are always part of it.
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
.if !target(tags) && defined(SRCS) && !defined(NO_TAGS)
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

.if !exists(${.OBJDIR}/${DEPENDFILE})
.for _S in ${SRCS:N*.[hly]}
${_S:R}.o: ${_S}
.endfor
.endif

.for _LSRC in ${SRCS:M*.l:N*/*}
.for _LC in ${_LSRC:R}.c
${_LC}: ${_LSRC}
	${LEX} -t ${LFLAGS} ${.ALLSRC} > ${.TARGET}
.if !exists(${.OBJDIR}/${DEPENDFILE})
${_LC:R}.o: ${_LC}
.endif
SRCS:=	${SRCS:S/${_LSRC}/${_LC}/}
CLEANFILES+= ${_LC}
.endfor
.endfor

.for _YSRC in ${SRCS:M*.y:N*/*}
.for _YC in ${_YSRC:R}.c
SRCS:=	${SRCS:S/${_YSRC}/${_YC}/}
CLEANFILES+= ${_YC}
.if !empty(YFLAGS:M-d) && !empty(SRCS:My.tab.h)
.ORDER: ${_YC} y.tab.h
${_YC} y.tab.h: ${_YSRC}
	${YACC} ${YFLAGS} ${.ALLSRC}
	cp y.tab.c ${_YC}
CLEANFILES+= y.tab.c y.tab.h
.elif !empty(YFLAGS:M-d)
.for _YH in ${_YC:R}.h
.ORDER: ${_YC} ${_YH}
${_YC} ${_YH}: ${_YSRC}
	${YACC} ${YFLAGS} -o ${_YC} ${.ALLSRC}
SRCS+=	${_YH}
CLEANFILES+= ${_YH}
.endfor
.else
${_YC}: ${_YSRC}
	${YACC} ${YFLAGS} -o ${_YC} ${.ALLSRC}
.endif
.if !exists(${.OBJDIR}/${DEPENDFILE})
${_YC:R}.o: ${_YC}
.endif
.endfor
.endfor
.endif

.if !target(depend)
.if defined(SRCS)
depend: beforedepend ${DEPENDFILE} afterdepend

# Different types of sources are compiled with slightly different flags.
# Split up the sources, and filter out headers and non-applicable flags.
.if ${CC} == "icc"
MKDEP_CFLAGS=	${CFLAGS:M-X*} ${CFLAGS:M-[BID]*}
MKDEP_CXXFLAGS=	${CXXFLAGS:M-X*} ${CXXFLAGS:M-[BID]*}
MKDEP_OBJCFLAGS=${OBJCFLAGS:M-X*} ${OBJCFLAGS:M-[BID]*}
.else
MKDEP_CFLAGS=	${CFLAGS:M-nostdinc*} ${CFLAGS:M-[BID]*}
MKDEP_CXXFLAGS=	${CXXFLAGS:M-nostdinc*} ${CXXFLAGS:M-[BID]*}
MKDEP_OBJCFLAGS=${OBJCFLAGS:M-nostdinc*} ${OBJCFLAGS:M-[BID]*} ${OBJCFLAGS:M-Wno-import*}
.endif

DPSRCS+= ${SRCS}
${DEPENDFILE}: ${DPSRCS}
	rm -f ${DEPENDFILE}
.if !empty(DPSRCS:M*.[cS])
	${MKDEPCMD} -f ${DEPENDFILE} -a ${MKDEP} \
	    ${MKDEP_CFLAGS} ${.ALLSRC:M*.[cS]}
.endif
.if !empty(DPSRCS:M*.cc) || !empty(DPSRCS:M*.C) || !empty(DPSRCS:M*.cpp) || \
    !empty(DPSRCS:M*.cxx)
	${MKDEPCMD} -f ${DEPENDFILE} -a ${MKDEP} \
	    ${MKDEP_CXXFLAGS} \
	    ${.ALLSRC:M*.cc} ${.ALLSRC:M*.C} ${.ALLSRC:M*.cpp} ${.ALLSRC:M*.cxx}
.endif
.if !empty(DPSRCS:M*.m)
	${MKDEPCMD} -f ${DEPENDFILE} -a ${MKDEP} \
	    ${MKDEP_OBJCFLAGS} ${.ALLSRC:M*.m}
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
_LDADD_FROM_DPADD=	${DPADD:C;^/usr/lib/lib(.*)\.a$;-l\1;}
_LDADD_CANONICALIZED=	${LDADD:S/$//}
checkdpadd:
.if ${_LDADD_FROM_DPADD} != ${_LDADD_CANONICALIZED}
	@echo ${.CURDIR}
	@echo "DPADD -> ${_LDADD_FROM_DPADD}"
	@echo "LDADD -> ${_LDADD_CANONICALIZED}"
.endif
.endif
