#	$Id: bsd.obj.mk,v 1.1 1996/03/24 16:37:36 wosch Exp wosch $

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

cleanobj:
	rm -f -r ${.CURDIR}/${__objdir}
