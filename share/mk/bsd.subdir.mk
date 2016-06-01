#	from: @(#)bsd.subdir.mk	5.9 (Berkeley) 2/1/91
# $FreeBSD$
#
# The include file <bsd.subdir.mk> contains the default targets
# for building subdirectories.
#
# For all of the directories listed in the variable SUBDIRS, the
# specified directory will be visited and the target made. There is
# also a default target which allows the command "make subdir" where
# subdir is any directory listed in the variable SUBDIRS.
#
#
# +++ variables +++
#
# DISTRIBUTION	Name of distribution. [base]
#
# SUBDIR	A list of subdirectories that should be built as well.
#		Each of the targets will execute the same target in the
#		subdirectories. SUBDIR.yes is automatically appended
#		to this list.
#
# +++ targets +++
#
#	distribute:
# 		This is a variant of install, which will
# 		put the stuff into the right "distribution".
#
# 	See SUBDIR_TARGETS for list of targets that will recurse.
#
# 	Targets defined in STANDALONE_SUBDIR_TARGETS will always be ran
# 	with SUBDIR_PARALLEL and will not respect .WAIT or SUBDIR_DEPEND_
# 	values.
#
# 	SUBDIR_TARGETS and STANDALONE_SUBDIR_TARGETS can be appended to
# 	via make.conf or src.conf.
#

.if !target(__<bsd.subdir.mk>__)
__<bsd.subdir.mk>__:

SUBDIR_TARGETS+= \
		all all-man analyze buildconfig buildfiles buildincludes \
		checkdpadd clean cleandepend cleandir cleanilinks \
		cleanobj depend distribute files includes installconfig \
		installfiles installincludes realinstall lint maninstall \
		manlint obj objlink tags \

# Described above.
STANDALONE_SUBDIR_TARGETS+= \
		all-man buildconfig buildfiles buildincludes check checkdpadd \
		clean cleandepend cleandir cleanilinks cleanobj files includes \
		installconfig installincludes installfiles maninstall manlint \
		obj objlink \

# It is safe to install in parallel when staging.
.if defined(NO_ROOT)
STANDALONE_SUBDIR_TARGETS+= realinstall
.endif

.include <bsd.init.mk>

.if !defined(NEED_SUBDIR)
.if ${.MAKE.LEVEL} == 0 && ${MK_DIRDEPS_BUILD} == "yes" && !empty(SUBDIR) && !(make(clean*) || make(destroy*))
.include <meta.subdir.mk>
# ignore this
_SUBDIR:
.endif
.endif

DISTRIBUTION?=	base
.if !target(distribute)
distribute: .MAKE
.for dist in ${DISTRIBUTION}
	${_+_}cd ${.CURDIR}; \
	    ${MAKE} install installconfig -DNO_SUBDIR DESTDIR=${DISTDIR}/${dist} SHARED=copies
.endfor
.endif
# Convenience targets to run 'build${target}' and 'install${target}' when
# calling 'make ${target}'.
.for __target in files includes
.if !target(${__target})
${__target}:	build${__target} install${__target}
.ORDER:		build${__target} install${__target}
.endif
.endfor

# Make 'install' supports a before and after target.  Actual install
# hooks are placed in 'realinstall'.
.if !target(install)
.for __stage in before real after
.if !target(${__stage}install)
${__stage}install:
.endif
.endfor
install:	beforeinstall realinstall afterinstall
.ORDER:		beforeinstall realinstall afterinstall
.endif
.ORDER: all install

# SUBDIR recursing may be disabled for MK_DIRDEPS_BUILD
.if !target(_SUBDIR)

.if defined(SUBDIR)
SUBDIR:=${SUBDIR} ${SUBDIR.yes}
SUBDIR:=${SUBDIR:u}
.endif

# Subdir code shared among 'make <subdir>', 'make <target>' and SUBDIR_PARALLEL.
_SUBDIR_SH=	\
		if test -d ${.CURDIR}/$${dir}.${MACHINE_ARCH}; then \
			dir=$${dir}.${MACHINE_ARCH}; \
		fi; \
		${ECHODIR} "===> ${DIRPRFX}$${dir} ($${target})"; \
		cd ${.CURDIR}/$${dir}; \
		${MAKE} $${target} DIRPRFX=${DIRPRFX}$${dir}/

_SUBDIR: .USEBEFORE
.if defined(SUBDIR) && !empty(SUBDIR) && !defined(NO_SUBDIR)
	@${_+_}target=${.TARGET:realinstall=install}; \
	    for dir in ${SUBDIR:N.WAIT}; do ( ${_SUBDIR_SH} ); done
.endif

${SUBDIR:N.WAIT}: .PHONY .MAKE
	${_+_}@target=all; \
	    dir=${.TARGET}; \
	    ${_SUBDIR_SH};

.for __target in ${SUBDIR_TARGETS}
# Only recurse on directly-called targets.  I.e., don't recurse on dependencies
# such as 'install' becoming {before,real,after}install, just recurse
# 'install'.  Despite that, 'realinstall' is special due to ordering issues
# with 'afterinstall'.
.if !defined(NO_SUBDIR) && (make(${__target}) || \
    (${__target} == realinstall && make(install)))
# Can ordering be skipped for this and SUBDIR_PARALLEL forced?
.if ${STANDALONE_SUBDIR_TARGETS:M${__target}}
_is_standalone_target=	1
SUBDIR:=	${SUBDIR:N.WAIT}
.else
_is_standalone_target=	0
.endif
.if defined(SUBDIR_PARALLEL) || ${_is_standalone_target} == 1
__subdir_targets=
.for __dir in ${SUBDIR}
.if ${__dir} == .WAIT
__subdir_targets+= .WAIT
.else
__subdir_targets+= ${__target}_subdir_${DIRPRFX}${__dir}
__deps=
.if ${_is_standalone_target} == 0
.for __dep in ${SUBDIR_DEPEND_${__dir}}
__deps+= ${__target}_subdir_${DIRPRFX}${__dep}
.endfor
.endif
${__target}_subdir_${DIRPRFX}${__dir}: .PHONY .MAKE .SILENT ${__deps}
	@${_+_}target=${__target:realinstall=install}; \
	    dir=${__dir}; \
	    ${_SUBDIR_SH};
.endif
.endfor	# __dir in ${SUBDIR}
${__target}: ${__subdir_targets}
.else
${__target}: _SUBDIR
.endif	# SUBDIR_PARALLEL || _is_standalone_target
.endif	# make(${__target})
.endfor	# __target in ${SUBDIR_TARGETS}

.endif	# !target(_SUBDIR)

# Ensure all targets exist
.for __target in ${SUBDIR_TARGETS}
.if !target(${__target})
${__target}:
.endif
.endfor

.endif
