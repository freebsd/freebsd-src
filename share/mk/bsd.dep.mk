#	$Id: bsd.dep.mk,v 1.5 1996/03/31 20:04:08 wosch Exp $

MKDEPCMD?=	mkdep
DEPENDFILE?=	.depend

# some of the rules involve .h sources, so remove them from mkdep line
.if !target(depend)
depend: beforedepend ${DEPENDFILE} afterdepend ${_DEPSUBDIR}
.if defined(SRCS)

# .if defined ${SRCS:M*.[sS]} does not work
__depend_s=	${SRCS:M*.[sS]}
__depend_c=	${SRCS:M*.c}
__depend_cc=	${SRCS:M*.cc} ${SRCS:M*.C} ${SRCS:M*.cxx}

${DEPENDFILE}: ${SRCS}
	rm -f ${DEPENDFILE}
.if defined(__depend_s) && !empty(__depend_s)
	${MKDEPCMD} -f ${DEPENDFILE} -a ${MKDEP} ${CFLAGS:M-[ID]*} ${AINC} \
		${.ALLSRC:M*.[sS]}
.endif
.if defined(__depend_c) && !empty(__depend_c)
	${MKDEPCMD} -f ${DEPENDFILE} -a ${MKDEP} ${CFLAGS:M-[ID]*} \
		${.ALLSRC:M*.c}
.endif
.if defined(__depend_cc) && !empty(__depend_cc)
	${MKDEPCMD} -f ${DEPENDFILE} -a ${MKDEP} \
		${CXXFLAGS:M-nostd*} ${CXXFLAGS:M-[ID]*} \
		${.ALLSRC:M*.cc} ${.ALLSRC:M*.C} ${.ALLSRC:M*.cxx}
.endif

.else
${DEPENDFILE}: ${_DEPSUBDIR}
.endif
.if !target(beforedepend)
beforedepend:
.endif
.if !target(afterdepend)
afterdepend:
.endif
.endif

.if !target(tags)
.if defined(SRCS)
tags: ${SRCS}
	-cd ${.CURDIR}; ctags -f /dev/stdout ${.ALLSRC:N*.h} | \
	    sed "s;\${.CURDIR}/;;" > tags
.else
tags:
.endif
.endif

.if defined(SRCS)
clean:
cleandir: cleandepend
cleandepend:
	rm -f ${DEPENDFILE} ${.CURDIR}/tags
.endif
