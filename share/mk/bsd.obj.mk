#	$Id: bsd.obj.mk,v 1.19 1997/02/22 13:56:12 peter Exp $
#
# The include file <bsd.obj.mk> handles creating the 'obj' directory
# and cleaning up object files, log files etc.
#
#
# +++ variables +++
#
# CLEANFILES	Additional files to remove for the clean and cleandir targets.
#
# MAKEOBJDIR 	A pathname for the directory where the targets 
#		are built.  Note: MAKEOBJDIR is an *enviroment* variable
#		and works properly only if set as an enviroment variable,
#		not as a global or command line variable!
#
#		E.g. use `env MAKEOBJDIR=temp-obj make'
#
# MAKEOBJDIRPREFIX  Specifies somewhere other than /usr/obj to root the object
#		tree.  Note: MAKEOBJDIRPREFIX is an *enviroment* variable
#		and works properly only if set as an enviroment variable,
#		not as a global or command line variable!
#
#		E.g. use `env MAKEOBJDIRPREFIX=/somewhere/obj make'
#
# NOOBJ		Do not create object directories.  This should not be set
#		if anything is built.
#
# OBJLINK	Create a symbolic link from ${.CURDIR}/obj to
#		${CANONICALOBJDIR}.  Note: this BREAKS the read-only source
#		tree rule!
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
.if !defined(NOOBJ) && ${.OBJDIR} != ${CANONICALOBJDIR}
.if ${.OBJDIR} == ${.CURDIR}
	@${ECHO} "Warning: Object directory not changed from original ${.CURDIR}"
.elif !defined(MAKEOBJDIR) && !defined(MAKEOBJDIRPREFIX) && !defined(OBJLINK)
	@${ECHO} "Warning: Using ${.OBJDIR} as object directory instead of\
		canonical ${CANONICALOBJDIR}"
.endif
.endif

.if !target(obj)
.if defined(NOOBJ)
obj:
.else
.if !defined(OBJLINK)
obj:	_SUBDIR
	@if ! test -d ${CANONICALOBJDIR}/; then \
		mkdir -p ${CANONICALOBJDIR}; \
		if ! test -d ${CANONICALOBJDIR}/; then \
			${ECHO} "Unable to create ${CANONICALOBJDIR}."; \
			exit 1; \
		fi; \
		${ECHO} "${CANONICALOBJDIR} created for ${.CURDIR}"; \
	fi
.else
obj:	_SUBDIR
	@if ! test -d ${CANONICALOBJDIR}/; then \
		mkdir -p ${CANONICALOBJDIR}; \
		if ! test -d ${CANONICALOBJDIR}/; then \
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
	@if test -d ${CANONICALOBJDIR}/; then \
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
	@cd ${.CURDIR}; ${MAKE} -V .OBJDIR
.endif

#
# cleanup
#
cleanobj:
	@if [ -d ${CANONICALOBJDIR}/ ]; then \
		rm -rf ${CANONICALOBJDIR}; \
	else \
		cd ${.CURDIR} && ${MAKE} clean cleandepend; \
	fi
	@if [ -h ${.CURDIR}/obj ]; then rm -f ${.CURDIR}/obj; fi

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

.if !target(checkdpadd)
checkdpadd: _SUBDIR
.if (defined(DPADD) || defined(LDADD))
checkdpadd:
	@if [ "${DPADD:S;^/usr/lib/lib;-l;S;.a$;;}" != "${LDADD}" ] ; then \
		echo ${.CURDIR} ; \
		echo "DPADD -> " ${DPADD:S;^/usr/lib/lib;-l;S;.a$;;} ; \
		echo "LDADD =  " ${LDADD} ; \
	fi
.endif
.endif

cleandir: cleanobj _SUBDIR
