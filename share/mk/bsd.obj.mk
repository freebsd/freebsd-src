# $FreeBSD$
#
# The include file <bsd.obj.mk> handles creating the 'obj' directory
# and cleaning up object files, etc.
#
# Under construction: it also contains the _SUBDIR target (which is used
# by most `mk' files to recurse into subdirectories) and defaults for the
# cleandepend, depend and tags targets.  It may eventually be merged with
# with bsd.subdir.mk.
#
# +++ variables +++
#
# CLEANDIRS	Additional directories to remove for the clean target.
#
# CLEANFILES	Additional files to remove for the clean target.
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

.if !target(obj)
.if defined(NOOBJ)
obj:
.else
obj:	_SUBDIR
	@if ! test -d ${CANONICALOBJDIR}/; then \
		mkdir -p ${CANONICALOBJDIR}; \
		if ! test -d ${CANONICALOBJDIR}/; then \
			${ECHO} "Unable to create ${CANONICALOBJDIR}."; \
			exit 1; \
		fi; \
		${ECHO} "${CANONICALOBJDIR} created for ${.CURDIR}"; \
	fi
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
	@echo ${.OBJDIR}
.endif

cleanobj:
.if ${CANONICALOBJDIR} != ${.CURDIR} && exists(${CANONICALOBJDIR}/)
	@rm -rf ${CANONICALOBJDIR}
.else
	@cd ${.CURDIR} && ${MAKE} clean cleandepend
.endif
	@if [ -h ${.CURDIR}/obj ]; then rm -f ${.CURDIR}/obj; fi

.if !target(clean)
clean: _SUBDIR
.if defined(CLEANFILES) && !empty(CLEANFILES)
	rm -f ${CLEANFILES} 
.endif
.if defined(CLEANDIRS) && !empty(CLEANDIRS)
	rm -rf ${CLEANDIRS}
.endif
.endif

.if !target(checkdpadd)
checkdpadd: _SUBDIR
.if (defined(DPADD) || defined(LDADD))
checkdpadd:
.if ${OBJFORMAT} != aout
	@ldadd=`echo \`for lib in ${DPADD} ; do \
		echo $$lib | sed 's;^/usr/lib/lib\(.*\)\.a;-l\1;' ; \
	done \`` ; \
	ldadd1=`echo ${LDADD}` ; \
	if [ "$$ldadd" != "$$ldadd1" ] ; then \
		echo ${.CURDIR} ; \
		echo "DPADD -> $$ldadd" ; \
		echo "LDADD -> $$ldadd1" ; \
	fi
.else
	@dpadd=`echo \`ld -Bstatic -f ${LDADD}\`` ; \
	if [ "$$dpadd" != "${DPADD}" ] ; then \
		echo ${.CURDIR} ; \
		echo "LDADD -> $$dpadd" ; \
		echo "DPADD =  ${DPADD}" ; \
	fi
.endif
.endif
.endif

cleandir: cleanobj _SUBDIR

.for __target in cleandepend depend tags
.if !target(${__target})
${__target}: _SUBDIR
.endif
.endfor

_SUBDIR: .USE
.if defined(SUBDIR) && !empty(SUBDIR)
	@for entry in ${SUBDIR}; do \
		(${ECHODIR} "===> ${DIRPRFX}$$entry"; \
		if test -d ${.CURDIR}/$${entry}.${MACHINE_ARCH}; then \
			cd ${.CURDIR}/$${entry}.${MACHINE_ARCH}; \
		else \
			cd ${.CURDIR}/$${entry}; \
		fi; \
		${MAKE} ${.TARGET:S/realinstall/install/} DIRPRFX=${DIRPRFX}$$entry/); \
	done
.endif
