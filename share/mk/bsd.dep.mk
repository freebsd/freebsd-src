#	$Id: bsd.dep.mk,v 1.4 1994/02/27 19:28:44 nate Exp $

# some of the rules involve .h sources, so remove them from mkdep line
.if !target(depend)
depend: beforedepend .depend afterdepend
.if defined(SRCS)
.depend: ${SRCS}
	rm -f .depend
	files="${.ALLSRC:M*.[sS]}"; \
	if [ "$$files" != "" ]; then \
	  mkdep -a ${MKDEP} ${CFLAGS:M-[ID]*} ${AINC} $$files; \
	fi
	files="${.ALLSRC:M*.c}"; \
	if [ "$$files" != "" ]; then \
	  mkdep -a ${MKDEP} ${CFLAGS:M-[ID]*} $$files; \
	fi
	files="${.ALLSRC:M*.cc} ${.ALLSRC:M*.C} ${.ALLSRC:M*.cxx}"; \
	if [ "$$files" != "  " ]; then \
	  mkdep -a ${MKDEP} ${CXXFLAGS:M-nostd*} ${CXXFLAGS:M-[ID]*} $$files; \
	fi
.else
.depend:
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
	rm -f .depend ${.CURDIR}/tags
.endif
