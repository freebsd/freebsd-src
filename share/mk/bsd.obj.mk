# $FreeBSD$
#
# The include file <bsd.obj.mk> handles creating the 'obj' directory
# and cleaning up object files, etc.
#
# +++ variables +++
#
# CLEANDIRS	Additional directories to remove for the clean target.
#
# CLEANFILES	Additional files to remove for the clean target.
#
# MAKEOBJDIR 	A pathname for the directory where the targets 
#		are built.  Note: MAKEOBJDIR is an *environment* variable
#		and works properly only if set as an environment variable,
#		not as a global or command line variable!
#
#		E.g. use `env MAKEOBJDIR=temp-obj make'
#
# MAKEOBJDIRPREFIX  Specifies somewhere other than /usr/obj to root the object
#		tree.  Note: MAKEOBJDIRPREFIX is an *environment* variable
#		and works properly only if set as an environment variable,
#		not as a global or command line variable!
#
#		E.g. use `env MAKEOBJDIRPREFIX=/somewhere/obj make'
#
# NOOBJ		Do not create object directories.  This should not be set
#		if anything is built.
#
# +++ targets +++
#
#	clean:
#		remove ${CLEANFILES}; remove ${CLEANDIRS} and all contents.
#
#	cleandir:
#		remove the build directory (and all its contents) created by obj
#
#	obj:
#		create build directory.
#

.if !target(__<bsd.obj.mk>__)
__<bsd.obj.mk>__:
.include <bsd.own.mk>

.if defined(MAKEOBJDIRPREFIX)
CANONICALOBJDIR:=${MAKEOBJDIRPREFIX}${.CURDIR}
.else
CANONICALOBJDIR:=/usr/obj${.CURDIR}
.endif

#
# Warn of unorthodox object directory.
#
# The following directories are tried in order for ${.OBJDIR}:
#
# 1.  ${MAKEOBJDIRPREFIX}/`pwd`
# 2.  ${MAKEOBJDIR}
# 3.  obj.${MACHINE}
# 4.  obj
# 5.  /usr/obj/`pwd`
# 6.  ${.CURDIR}
#
# If ${.OBJDIR} is constructed using canonical cases 1 or 5, or
# case 2 (using MAKEOBJDIR), don't issue a warning.  Otherwise,
# issue a warning differentiating between cases 6 and (3 or 4).
#
objwarn:
.if !defined(NOOBJ) && ${.OBJDIR} != ${CANONICALOBJDIR} && \
    !(defined(MAKEOBJDIRPREFIX) && exists(${CANONICALOBJDIR}/)) && \
    !(defined(MAKEOBJDIR) && exists(${MAKEOBJDIR}/))
.if ${.OBJDIR} == ${.CURDIR}
	@${ECHO} "Warning: Object directory not changed from original ${.CURDIR}"
.elif exists(${.CURDIR}/obj.${MACHINE}/) || exists(${.CURDIR}/obj/)
	@${ECHO} "Warning: Using ${.OBJDIR} as object directory instead of\
		canonical ${CANONICALOBJDIR}"
.endif
.endif

.if !defined(NOOBJ)
.if !target(obj)
obj:
	@if ! test -d ${CANONICALOBJDIR}/; then \
		mkdir -p ${CANONICALOBJDIR}; \
		if ! test -d ${CANONICALOBJDIR}/; then \
			${ECHO} "Unable to create ${CANONICALOBJDIR}."; \
			exit 1; \
		fi; \
		${ECHO} "${CANONICALOBJDIR} created for ${.CURDIR}"; \
	fi
.endif

.if !target(objlink)
objlink:
	@if test -d ${CANONICALOBJDIR}/; then \
		rm -f ${.CURDIR}/obj; \
		ln -s ${CANONICALOBJDIR} ${.CURDIR}/obj; \
	else \
		echo "No ${CANONICALOBJDIR} to link to - do a make obj."; \
	fi
.endif
.endif !defined(NOOBJ)

#
# where would that obj directory be?
#
.if !target(whereobj)
whereobj:
	@echo ${.OBJDIR}
.endif

cleanobj:
.if ${CANONICALOBJDIR} != ${.CURDIR} && exists(${CANONICALOBJDIR}/)
	@rm -rf ${CANONICALOBJDIR}
.else
	@cd ${.CURDIR} && ${MAKE} clean cleandepend
.endif
	@if [ -L ${.CURDIR}/obj ]; then rm -f ${.CURDIR}/obj; fi

.if !target(clean)
clean:
.if defined(CLEANFILES) && !empty(CLEANFILES)
	rm -f ${CLEANFILES} 
.endif
.if defined(CLEANDIRS) && !empty(CLEANDIRS)
	rm -rf ${CLEANDIRS}
.endif
.endif

cleandir: cleanobj

.include <bsd.subdir.mk>

.endif !target(__<bsd.obj.mk>__)
