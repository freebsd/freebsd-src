#	$Id: bsd.obj.mk,v 1.3 1996/04/22 23:31:39 wosch Exp $
#
# The include file <bsd.obj.mk> handles creating 'obj' directory
# and cleaning up object files, log files etc.
#
#
# +++ variables +++
#
# BSDSRCDIR	The real path to the system sources, so that 'make obj'
#		will work correctly. [/usr/src]
#
# BSDOBJDIR	The real path to the system 'obj' tree, so that 'make obj'
#		will work correctly. [/usr/obj]
#
# CLEANFILES	Additional files to remove for the clean and cleandir targets.
#
# MAKEOBJDIR 	A file name to the directory where the targets 
#		are built. Note: MAKEOBJDIR is an *enviroment* variable
#		and does work proper only if set as enviroment variable,
#		not as global or command line variable! [obj]
#
#		E.g. use `env MAKEOBJDIR=obj-amd make'
#
# NOOBJ		Do not create 'obj' directory if defined. [not set]
#
# NOOBJLINK	Create 'obj' directory in current directory instead
#		a symbolic link to the 'obj' tree if defined. [not set]
#
#
# +++ targets +++
#
#	clean:
#		remove a.out Errs errs mklog ${CLEANFILES} 
#
#	cleandir:
#		remove all of the files removed by the target clean, 
#		cleandepend (see bsd.dep.mk) and 'obj' directory.
#
#	obj:
#		create 'obj' directory.
#


.if defined(MAKEOBJDIR) && !empty(MAKEOBJDIR)
__objdir = ${MAKEOBJDIR}
.else

.if defined(MACHINE) && !empty(MACHINE)
__objdir = obj 			# obj.${MACHINE}
.else
__objdir = obj
.endif
.endif


.if !target(obj)
.if defined(NOOBJ)
obj:
.else

obj:	_SUBDIRUSE cleanobj
.if defined(NOOBJLINK)
	mkdir ${.CURDIR}/${__objdir}
.else
	@if test -d ${BSDOBJDIR}; then 			\
		cd ${.CURDIR}; here=${.CURDIR}; 	\
		dest=${BSDOBJDIR}`echo $$here |         \
			sed "s,^${BSDSRCDIR},,"`/${__objdir}; \
		${ECHO} "$$here/${__objdir} -> $$dest"; \
		ln -s $$dest ${__objdir}; 		\
		if test ! -d $$dest; then 		\
			mkdir -p $$dest; 		\
		fi; 					\
	else 						\
		${ECHO} "obj tree \"${BSDOBJDIR}\" does not exist."; \
	fi
.endif
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
