#	$Id: bsd.obj.mk,v 1.5 1996/06/24 04:24:06 jkh Exp $
#
# The include file <bsd.obj.mk> handles creating 'obj' directory
# and cleaning up object files, log files etc.
#
#
# +++ variables +++
#
# CLEANFILES	Additional files to remove for the clean and cleandir targets.
#
# MAKEOBJDIR 	Specify somewhere other than /usr/obj to root the object
#		tree. Note: MAKEOBJDIR is an *enviroment* variable
#		and does work proper only if set as enviroment variable,
#		not as global or command line variable! [obj]
#
#		E.g. use `env MAKEOBJDIR=/somewhere/obj make'
#
# NOOBJ		Do not create build directory in object tree.
#
# OBJLINK	Create a symbolic link from ${.TARGETOBJDIR} to ${.CURDIR}/obj
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


.if !target(obj)
.if defined(NOOBJ)
obj:
.else
.if !defined(OBJLINK)
obj:	_SUBDIR
	@if ! test -d ${.TARGETOBJDIR}; then \
		mkdir -p ${.TARGETOBJDIR}; \
		if ! test -d ${.TARGETOBJDIR}; then \
			${ECHO} "Unable to create ${.TARGETOBJDIR}."; \
			exit 1; \
		fi; \
		${ECHO} "${.TARGETOBJDIR} created for ${.CURDIR}"; \
	fi
.else
obj:	_SUBDIR
	@if ! test -d ${.TARGETOBJDIR}; then \
		mkdir -p ${.TARGETOBJDIR}; \
		if ! test -d ${.TARGETOBJDIR}; then \
			${ECHO} "Unable to create ${.TARGETOBJDIR}."; \
			exit 1; \
		fi; \
		ln -fs ${.TARGETOBJDIR} ${.CURDIR}/obj; \
		${ECHO} "${.CURDIR} -> ${.TARGETOBJDIR}"; \
	fi
.endif
.endif
.endif

.if !target(objlink)
objlink: _SUBDIR
	@if test -d ${.TARGETOBJDIR}; then \
		ln -fs ${.TARGETOBJDIR} ${.CURDIR}/obj; \
	else \
		echo "No ${.TARGETOBJDIR} to link to - do a make obj."; \
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
	@if ! test -d ${.TARGETOBJDIR}; then \
	    @echo ${.CURDIR}; \
	else
	    @echo ${.TARGETOBJDIR}; \
	fi
.endif
.endif

#
# cleanup
#
cleanobj:
	@if [ -d ${.TARGETOBJDIR} ]; then \
		rm -rf ${.TARGETOBJDIR}; \
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
