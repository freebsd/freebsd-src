#	$Id: bsd.dep.mk,v 1.17 1998/02/20 14:32:30 bde Exp $
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


MKDEPCMD?=	mkdep
DEPENDFILE?=	.depend

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
.if ${SRCS:M*.cc} != "" || ${SRCS:M*.C} != "" || ${SRCS:M*.cxx} != ""
	${MKDEPCMD} -f ${DEPENDFILE} -a ${MKDEP} \
	    ${CXXFLAGS:M-nostdinc*} ${CXXFLAGS:M-[BID]*} \
	    ${.ALLSRC:M*.cc} ${.ALLSRC:M*.C} ${.ALLSRC:M*.cxx}
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
	@cd ${.CURDIR} && gtags ${GTAGSFLAGS}
.if defined(HTML)
	@cd ${.CURDIR} && htags ${HTAGSFLAGS}
.endif
.endif

.if !target(cleandepend)
cleandepend: _SUBDIR
.if defined(SRCS)
	rm -f ${DEPENDFILE} ${.CURDIR}/GRTAGS ${.CURDIR}/GSYMS ${.CURDIR}/GTAGS
.if defined(HTML)
	rm -rf ${.CURDIR}/HTML
.endif
.endif
.endif

_SUBDIR: .USE
.if defined(SUBDIR) && !empty(SUBDIR)
	@for entry in ${SUBDIR}; do \
		(${ECHODIR} "===> ${DIRPRFX}$$entry"; \
		if test -d ${.CURDIR}/$${entry}.${MACHINE}; then \
			cd ${.CURDIR}/$${entry}.${MACHINE}; \
		else \
			cd ${.CURDIR}/$${entry}; \
		fi; \
		${MAKE} ${.TARGET:S/realinstall/install/:S/.depend/depend/} DIRPRFX=${DIRPRFX}$$entry/); \
	done
.endif
