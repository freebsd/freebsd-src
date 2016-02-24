# $FreeBSD$
#
# The include file <bsd.dep.mk> handles Makefile dependencies.
#
#
# +++ variables +++
#
# CLEANDEPENDDIRS	Additional directories to remove for the cleandepend
# 			target.
#
# CLEANDEPENDFILES	Additional files to remove for the cleandepend target.
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
#		remove ${CLEANDEPENDFILES}; remove ${CLEANDEPENDDIRS} and all
#		contents.
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

# XXX-BD: Is _MKDEPCC:= in head, but we need to expand in the target
_MKDEPCC=	${CC.${.IMPSRC:T}:U${CC}:N${CCACHE_BIN}}
# XXX: DEPFLAGS can come out once Makefile.inc1 properly passes down
# CXXFLAGS.
.if !empty(DEPFLAGS)
_MKDEPCC+=	${DEPFLAGS}
.endif
MKDEPCMD?=	CC='${_MKDEPCC}' mkdep
DEPENDFILE?=	.depend
.if ${MK_DIRDEPS_BUILD} == "no"
.MAKE.DEPENDFILE= ${DEPENDFILE}
.endif
CLEANDEPENDFILES=	${DEPENDFILE} ${DEPENDFILE}.*

# Keep `tags' here, before SRCS are mangled below for `depend'.
.if !target(tags) && defined(SRCS) && !defined(NO_TAGS)
tags: ${SRCS}
.if ${CTAGS:T} == "gtags"
	@cd ${.CURDIR} && ${CTAGS} ${GTAGSFLAGS} ${.OBJDIR}
.if defined(HTML)
	@cd ${.CURDIR} && htags ${HTAGSFLAGS} -d ${.OBJDIR} ${.OBJDIR}
.endif
.else
	@${CTAGS} ${CTAGSFLAGS} -f /dev/stdout \
	    ${.ALLSRC:N*.h} | sed "s;${.CURDIR}/;;" > ${.TARGET}
.endif
.endif

# Skip reading .depend when not needed to speed up tree-walks
# and simple lookups.
.if !empty(.MAKEFLAGS:M-V${_V_READ_DEPEND}) || make(obj) || make(clean*) || \
    make(install*)
_SKIP_READ_DEPEND=	1
.if ${MK_DIRDEPS_BUILD} == "no"
.MAKE.DEPENDFILE=	/dev/null
.endif
.endif

.if defined(SRCS)
CLEANFILES?=

.for _S in ${SRCS:N*.[dhly]}
OBJS_DEPEND_GUESS.${_S:R}.o=	${_S}
.if ${MK_FAST_DEPEND} == "no" && !exists(${.OBJDIR}/${DEPENDFILE})
${_S:R}.o: ${OBJS_DEPEND_GUESS.${_S:R}.o}
.endif
.endfor

# Lexical analyzers
.for _LSRC in ${SRCS:M*.l:N*/*}
.for _LC in ${_LSRC:R}.c
${_LC}: ${_LSRC}
	${LEX} ${LFLAGS} -o${.TARGET} ${.ALLSRC}
OBJS_DEPEND_GUESS.${_LC:R}.o=	${_LC}
.if ${MK_FAST_DEPEND} == "no" && !exists(${.OBJDIR}/${DEPENDFILE})
${_LC:R}.o: ${OBJS_DEPEND_GUESS.${_LC:R}.o}
.endif
SRCS:=	${SRCS:S/${_LSRC}/${_LC}/}
CLEANFILES+= ${_LC}
.endfor
.endfor

# Yacc grammars
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
OBJS_DEPEND_GUESS.${_YC:R}.o=	${_YC}
.if ${MK_FAST_DEPEND} == "no" && !exists(${.OBJDIR}/${DEPENDFILE})
${_YC:R}.o: ${OBJS_DEPEND_GUESS.${_YC:R}.o}
.endif
.endfor
.endfor

# DTrace probe definitions
.if ${SRCS:M*.d}
CFLAGS+=	-I${.OBJDIR}
.endif
.for _DSRC in ${SRCS:M*.d:N*/*}
.for _D in ${_DSRC:R}
SRCS+=	${_D}.h
${_D}.h: ${_DSRC}
	${DTRACE} ${DTRACEFLAGS} -h -s ${.ALLSRC}
SRCS:=	${SRCS:S/^${_DSRC}$//}
OBJS+=	${_D}.o
CLEANFILES+= ${_D}.h ${_D}.o
${_D}.o: ${_DSRC} ${OBJS:S/^${_D}.o$//}
	@rm -f ${.TARGET}
	${DTRACE} ${DTRACEFLAGS} -G -o ${.TARGET} -s ${.ALLSRC:N*.h}
.if defined(LIB)
CLEANFILES+= ${_D}.So ${_D}.po
${_D}.So: ${_DSRC} ${SOBJS:S/^${_D}.So$//}
	@rm -f ${.TARGET}
	${DTRACE} ${DTRACEFLAGS} -G -o ${.TARGET} -s ${.ALLSRC:N*.h}
${_D}.po: ${_DSRC} ${POBJS:S/^${_D}.po$//}
	@rm -f ${.TARGET}
	${DTRACE} ${DTRACEFLAGS} -G -o ${.TARGET} -s ${.ALLSRC:N*.h}
.endif
.endfor
.endfor


.if !empty(.MAKE.MODE:Mmeta) && empty(.MAKE.MODE:Mnofilemon)
_meta_filemon=	1
.endif
.if ${MK_FAST_DEPEND} == "yes"
DEPEND_MP?=	-MP
# Handle OBJS=../somefile.o hacks.  Just replace '/' rather than use :T to
# avoid collisions.
DEPEND_FILTER=	C,/,_,g
DEPENDSRCS=	${SRCS:M*.[cSC]} ${SRCS:M*.cxx} ${SRCS:M*.cpp} ${SRCS:M*.cc}
.if !empty(DEPENDSRCS)
DEPENDOBJS+=	${DEPENDSRCS:R:S,$,.o,}
.endif
DEPENDFILES_OBJS=	${DEPENDOBJS:O:u:${DEPEND_FILTER}:C/^/${DEPENDFILE}./}
DEPEND_CFLAGS+=	-MD ${DEPEND_MP} -MF${DEPENDFILE}.${.TARGET:${DEPEND_FILTER}}
DEPEND_CFLAGS+=	-MT${.TARGET}
# Skip generating or including .depend.* files if in meta+filemon mode since
# it will track dependencies itself.  OBJS_DEPEND_GUESS is still used though.
.if !defined(_meta_filemon)
.if defined(.PARSEDIR)
# Only add in DEPEND_CFLAGS for CFLAGS on files we expect from DEPENDOBJS
# as those are the only ones we will include.
DEPEND_CFLAGS_CONDITION= !empty(DEPENDOBJS:M${.TARGET:${DEPEND_FILTER}})
CFLAGS+=	${${DEPEND_CFLAGS_CONDITION}:?${DEPEND_CFLAGS}:}
.else
CFLAGS+=	${DEPEND_CFLAGS}
.endif
.if !defined(_SKIP_READ_DEPEND)
.for __depend_obj in ${DEPENDFILES_OBJS}
.sinclude "${__depend_obj}"
.endfor
.endif	# !defined(_SKIP_READ_DEPEND)
.endif	# !defined(_meta_filemon)
.endif	# ${MK_FAST_DEPEND} == "yes"
.endif	# defined(SRCS)

.if ${MK_DIRDEPS_BUILD} == "yes"
# Prevent meta.autodep.mk from tracking "local dependencies".
.depend:
.include <meta.autodep.mk>
# If using filemon then _EXTRADEPEND is skipped since it is not needed.
.if empty(.MAKE.MODE:Mnofilemon)
# this depend: bypasses that below
# the dependency helps when bootstrapping
depend: beforedepend ${DPSRCS} ${SRCS} afterdepend
beforedepend:
afterdepend: beforedepend
.endif
.endif

# Guess some dependencies for when no ${DEPENDFILE}.OBJ is generated yet.
# For meta+filemon the .meta file is checked for since it is the dependency
# file used.
.if ${MK_FAST_DEPEND} == "yes"
.for __obj in ${DEPENDOBJS:O:u}
.if (defined(_meta_filemon) && !exists(${.OBJDIR}/${__obj}.meta)) || \
    (!defined(_meta_filemon) && !exists(${.OBJDIR}/${DEPENDFILE}.${__obj}))
${__obj}: ${OBJS_DEPEND_GUESS}
${__obj}: ${OBJS_DEPEND_GUESS.${__obj}}
.endif
.endfor

# Always run 'make depend' to generate dependencies early and to avoid the
# need for manually running it.  The dirdeps build should only do this in
# sub-makes though since MAKELEVEL0 is for dirdeps calculations.
.if ${MK_DIRDEPS_BUILD} == "no" || ${.MAKE.LEVEL} > 0
beforebuild: depend
.endif
.endif	# ${MK_FAST_DEPEND} == "yes"

.if !target(depend)
.if defined(SRCS)
depend: beforedepend ${DEPENDFILE} afterdepend

# Tell bmake not to look for generated files via .PATH
.NOPATH: ${DEPENDFILE} ${DEPENDFILES_OBJS}

.if ${MK_FAST_DEPEND} == "no"
# Capture -include from CFLAGS.
# This could be simpler with bmake :tW but needs to support fmake for MFC.
_CFLAGS_INCLUDES= ${CFLAGS:Q:S/\\ /,/g:C/-include,/-include%/g:C/,/ /g:M-include*:C/%/ /g}
_CXXFLAGS_INCLUDES= ${CXXFLAGS:Q:S/\\ /,/g:C/-include,/-include%/g:C/,/ /g:M-include*:C/%/ /g}

# Different types of sources are compiled with slightly different flags.
# Split up the sources, and filter out headers and non-applicable flags.
MKDEP_CFLAGS=	${CFLAGS:M-nostdinc*} ${CFLAGS:M-[BIDU]*} ${CFLAGS:M-std=*} \
		${CFLAGS:M-ansi} ${_CFLAGS_INCLUDES}
MKDEP_CXXFLAGS=	${CXXFLAGS:M-nostdinc*} ${CXXFLAGS:M-[BIDU]*} \
		${CXXFLAGS:M-std=*} ${CXXFLAGS:M-ansi} ${CXXFLAGS:M-stdlib=*} \
		${_CXXFLAGS_INCLUDES}
.endif	# ${MK_FAST_DEPEND} == "no"

DPSRCS+= ${SRCS}
# FAST_DEPEND will only generate a .depend if _EXTRADEPEND is used but
# the target is created to allow 'make depend' to generate files.
${DEPENDFILE}: ${DPSRCS}
.if exists(${.OBJDIR}/${DEPENDFILE})
	rm -f ${DEPENDFILE}
.endif
.if ${MK_FAST_DEPEND} == "no"
.if !empty(DPSRCS:M*.[cS])
	${MKDEPCMD} -f ${DEPENDFILE} -a ${MKDEP} \
	    ${MKDEP_CFLAGS} ${.ALLSRC:M*.[cS]}
.endif
.if !empty(DPSRCS:M*.cc) || !empty(DPSRCS:M*.C) || !empty(DPSRCS:M*.cpp) || \
    !empty(DPSRCS:M*.cxx)
	${MKDEPCMD} -f ${DEPENDFILE} -a ${MKDEP} \
	    ${MKDEP_CXXFLAGS} \
	    ${.ALLSRC:M*.cc} ${.ALLSRC:M*.C} ${.ALLSRC:M*.cpp} ${.ALLSRC:M*.cxx}
.else
.endif
.endif	# ${MK_FAST_DEPEND} == "no"
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

.if defined(SRCS)
.if ${CTAGS:T} == "gtags"
CLEANDEPENDFILES+=	GPATH GRTAGS GSYMS GTAGS
.if defined(HTML)
CLEANDEPENDDIRS+=	HTML
.endif
.else
CLEANDEPENDFILES+=	tags
.endif
.endif
.if !target(cleandepend)
cleandepend:
.if !empty(CLEANDEPENDFILES)
	rm -f ${CLEANDEPENDFILES}
.endif
.if !empty(CLEANDEPENDDIRS)
	rm -rf ${CLEANDEPENDDIRS}
.endif
.endif

.if !target(checkdpadd) && (defined(DPADD) || defined(LDADD))
_LDADD_FROM_DPADD=	${DPADD:R:T:C;^lib(.*)$;-l\1;g}
# Ignore -Wl,--start-group/-Wl,--end-group as it might be required in the
# LDADD list due to unresolved symbols
_LDADD_CANONICALIZED=	${LDADD:N:R:T:C;^lib(.*)$;-l\1;g:N-Wl,--[es]*-group}
checkdpadd:
.if ${_LDADD_FROM_DPADD} != ${_LDADD_CANONICALIZED}
	@echo ${.CURDIR}
	@echo "DPADD -> ${_LDADD_FROM_DPADD}"
	@echo "LDADD -> ${_LDADD_CANONICALIZED}"
.endif
.endif
