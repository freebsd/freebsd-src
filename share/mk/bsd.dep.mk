#	$Id: bsd.dep.mk,v 1.12 1997/04/13 06:44:20 jkh Exp $
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

# some of the rules involve .h sources, so remove them from mkdep line
.if !target(depend)
depend: beforedepend ${DEPENDFILE} afterdepend _SUBDIR
.if defined(SRCS)

# .if defined ${SRCS:M*.[sS]} does not work
__depend_s=	${SRCS:M*.[sS]}
__depend_c=	${SRCS:M*.c}
__depend_cc=	${SRCS:M*.cc} ${SRCS:M*.C} ${SRCS:M*.cxx}

${DEPENDFILE}: ${SRCS}
	rm -f ${DEPENDFILE}
.if defined(__depend_s) && !empty(__depend_s)
	${MKDEPCMD} -f ${DEPENDFILE} -a ${MKDEP} ${CFLAGS:M-[BID]*} ${AINC} \
		${.ALLSRC:M*.[sS]}
.endif
.if defined(__depend_c) && !empty(__depend_c)
	${MKDEPCMD} -f ${DEPENDFILE} -a ${MKDEP} ${CFLAGS:M-[BID]*} \
		${.ALLSRC:M*.c}
.endif
.if defined(__depend_cc) && !empty(__depend_cc)
	${MKDEPCMD} -f ${DEPENDFILE} -a ${MKDEP} \
		${CXXFLAGS:M-nostd*} ${CXXFLAGS:M-[BID]*} \
		${.ALLSRC:M*.cc} ${.ALLSRC:M*.C} ${.ALLSRC:M*.cxx}
.endif
.if target(_EXTRADEPEND)
	cd ${.CURDIR}; ${MAKE} _EXTRADEPEND
.endif

.else
${DEPENDFILE}: _SUBDIR
.endif
.if !target(beforedepend)
beforedepend:
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

.if defined(SRCS)
.if !target(cleandepend)
cleandepend: _SUBDIR
	rm -f ${DEPENDFILE}
	rm -f ${.CURDIR}/GRTAGS ${.CURDIR}/GTAGS
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
