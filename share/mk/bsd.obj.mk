#	$Id: bsd.obj.mk,v 1.1 1996/03/24 22:49:16 wosch Exp $

# _SUBDIRUSE:
# BSDSRCDIR?=/usr/src
# BSDOBJDIR?=/usr/obj

.if defined(MAKEOBJDIR) && !empty(MAKEOBJDIR)
__objdir = ${MAKEOBJDIR}
.else

.if defined(MACHINE) && !empty(MACHINE)
__objdir = obj.${MACHINE}
.else
__objdir = obj
.endif
.endif


.if !target(obj)
.if defined(NOOBJ)
obj:
.else

obj:	_SUBDIRUSE cleanobj
	@cd ${.CURDIR}; here=${.CURDIR}; 		\
	dest=${BSDOBJDIR}`echo $$here | sed "s,^${BSDSRCDIR},,"`/${__objdir}; \
	if test -d ${BSDOBJDIR}; then 			\
		${ECHO} "$$here/${__objdir} -> $$dest"; \
		ln -s $$dest ${__objdir}; 		\
		if test ! -d $$dest; then 		\
			mkdir -p $$dest; 		\
		fi; 					\
	fi
.endif
.endif

#
# cleanup
#
cleanobj: 
	rm -f -r ${.CURDIR}/${__objdir}

cleanfiles:
	rm -f a.out Errs errs mklog ${CLEANFILES} 

# see bsd.dep.mk
.if !target(cleandepend)
cleandepend:
.endif


.if !target(clean)
clean: _SUBDIRUSE cleanfiles
.endif

cleandir: _SUBDIRUSE cleanfiles cleandepend cleanobj
