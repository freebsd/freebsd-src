#	$Id: bsd.obj.mk,v 1.8 1996/09/05 17:53:13 bde Exp $
#
# The include file <bsd.obj.mk> handles creating 'obj' directory
# and cleaning up object files, log files etc.
#
#
# +++ variables +++
#
# CLEANFILES	Additional files to remove for the clean and cleandir targets.
#
# MAKEOBJDIRPREFIX  Specify somewhere other than /usr/obj to root the object
#		tree. Note: MAKEOBJDIRPREFIX is an *enviroment* variable
#		and does work proper only if set as enviroment variable,
#		not as global or command line variable! [obj]
#
#		E.g. use `env MAKEOBJDIRPREFIX=/somewhere/obj make'
#
# NOOBJ		Do not create build directory in object tree.
#
# OBJLINK	Create a symbolic link from ${CANONICALOBJDIR} to ${.CURDIR}/obj
#		Note:  This BREAKS the read-only src tree rule!
#
# +++ targets +++
#
#	clean:
#		remove a.out Errs errs mklog ${CLEANFILES} 
#
#	cleandir:
#		remove the build directory (and all its contents) created by obj
#
#	obj:
#		create build directory.
#

.if defined(MAKEOBJDIRPREFIX)
CANONICALOBJDIR:=${MAKEOBJDIRPREFIX}${.CURDIR}
.else
CANONICALOBJDIR:=/usr/obj${.CURDIR}
.endif

#
# Warn of unorthodox object directory
#
objwarn:
.if ${.OBJDIR} == ${.CURDIR}
	${ECHO}	"Warning: Object directory not changed from original ${.CURDIR}"
.elif !defined(MAKEOBJDIRPREFIX) && ${.OBJDIR} != ${CANONICALOBJDIR}
.if !defined(MAKEOBJDIR)
	${ECHO} "Warning: Using ${.OBJDIR} as object directory instead of\
		canonical ${CANONICALOBJDIR}"
.endif
.endif



.if !target(obj)
.if defined(NOOBJ)
obj:
.else
.if !defined(OBJLINK)
obj:	_SUBDIR
	@if ! test -d ${CANONICALOBJDIR}; then \
		mkdir -p ${CANONICALOBJDIR}; \
		if ! test -d ${CANONICALOBJDIR}; then \
			${ECHO} "Unable to create ${CANONICALOBJDIR}."; \
			exit 1; \
		fi; \
		${ECHO} "${CANONICALOBJDIR} created for ${.CURDIR}"; \
	fi
.else
obj:	_SUBDIR
	@if ! test -d ${CANONICALOBJDIR}; then \
		mkdir -p ${CANONICALOBJDIR}; \
		if ! test -d ${CANONICALOBJDIR}; then \
			${ECHO} "Unable to create ${CANONICALOBJDIR}."; \
			exit 1; \
		fi; \
		rm -f ${.CURDIR}/obj; \
		ln -s ${CANONICALOBJDIR} ${.CURDIR}/obj; \
		${ECHO} "${.CURDIR} -> ${CANONICALOBJDIR}"; \
	fi
.endif
.endif
.endif

.if !target(objlink)
objlink: _SUBDIR
	@if test -d ${CANONICALOBJDIR}; then \
		rm -f ${.CURDIR}/obj; \
		ln -s ${CANONICALOBJDIR} ${.CURDIR}/obj; \
	else \
		echo "No ${CANONICALOBJDIR} to link to - do a make obj."; \
	fi
.endif

#
# where would that obj directory be?
#
.if !target(whereobj)
whereobj:
.if defined(NOOBJ)
	@echo ${.CURDIR}
.else
	@if ! test -d ${CANONICALOBJDIR}; then \
	    echo ${.CURDIR}; \
	else \
	    echo ${CANONICALOBJDIR}; \
	fi
.endif
.endif

#
# cleanup
#
cleanobj:
	@if [ -d ${CANONICALOBJDIR} ]; then \
		rm -rf ${CANONICALOBJDIR}; \
	else \
		cd ${.CURDIR} && ${MAKE} clean cleandepend; \
	fi
.if defined(OBJLINK)
	@if [ -h ${.CURDIR}/obj ]; then rm -f ${.CURDIR}/obj; fi
.endif

.if !target(cleanfiles)
cleanfiles:
	rm -f a.out Errs errs mklog ${CLEANFILES} 
.endif

# see bsd.dep.mk
.if !target(cleandepend)
cleandepend:
	@rm -f .depend
.endif

.if !target(clean)
clean: cleanfiles _SUBDIR
.endif

cleandir: cleanobj _SUBDIR
