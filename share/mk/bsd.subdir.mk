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
#		subdirectories. SUBDIR.yes is automatically appeneded
#		to this list.
#
# +++ targets +++
#
#	distribute:
# 		This is a variant of install, which will
# 		put the stuff into the right "distribution".
#
# 	See ALL_SUBDIR_TARGETS for list of targets that will recurse.
# 	Custom targets can be added to SUBDIR_TARGETS in src.conf.
#

.if !target(__<bsd.subdir.mk>__)
__<bsd.subdir.mk>__:

ALL_SUBDIR_TARGETS= all all-man buildconfig checkdpadd clean cleandepend \
		    cleandir cleanilinks cleanobj depend distribute \
		    installconfig lint maninstall manlint obj objlink \
		    realinstall regress tags \
		    ${SUBDIR_TARGETS}

.include <bsd.init.mk>

.if !defined(NEED_SUBDIR)
.if ${.MAKE.LEVEL} == 0 && ${MK_META_MODE} == "yes" && !empty(SUBDIR) && !(make(clean*) || make(destroy*))
.include <meta.subdir.mk>
# ignore this
_SUBDIR:
.endif
.endif
.if !target(_SUBDIR)

.if defined(SUBDIR)
SUBDIR:=${SUBDIR} ${SUBDIR.yes}
SUBDIR:=${SUBDIR:u}
.endif

DISTRIBUTION?=	base
.if !target(distribute)
distribute: .MAKE
.for dist in ${DISTRIBUTION}
	${_+_}cd ${.CURDIR}; \
	    ${MAKE} install -DNO_SUBDIR DESTDIR=${DISTDIR}/${dist} SHARED=copies
.endfor
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
	@${_+_}target=${.TARGET:S,realinstall,install,}; \
	    for dir in ${SUBDIR:N.WAIT}; do ${_SUBDIR_SH}; done
.endif

${SUBDIR:N.WAIT}: .PHONY .MAKE
	${_+_}@target=all; \
	    dir=${.TARGET}; \
	    ${_SUBDIR_SH};

# Work around parsing of .if nested in .for by putting .WAIT string into a var.
__wait= .WAIT
.for __target in ${ALL_SUBDIR_TARGETS}
.ifdef SUBDIR_PARALLEL
__subdir_targets=
.for __dir in ${SUBDIR}
.if ${__wait} == ${__dir}
__subdir_targets+= .WAIT
.else
__subdir_targets+= ${__target}_subdir_${__dir}
__deps=
.for __dep in ${SUBDIR_DEPEND_${__dir}}
__deps+= ${__target}_subdir_${__dep}
.endfor
${__target}_subdir_${__dir}: .PHONY .MAKE ${__deps}
.if !defined(NO_SUBDIR)
	@${_+_}target=${__target:realinstall=install}; \
	    dir=${__dir}; \
	    ${_SUBDIR_SH};
.endif
.endif
.endfor
${__target}: ${__subdir_targets}
.else
${__target}: _SUBDIR
.endif
.endfor

# This is to support 'make includes' calling 'make buildincludes' and
# 'make installincludes' in the proper order, and to support these
# targets as SUBDIR_TARGETS.
.for __target in files includes
.for __stage in build install
${__stage}${__target}:
.if make(${__stage}${__target})
${__stage}${__target}: _SUBDIR
.endif
.endfor
.if !target(${__target})
${__target}: .MAKE
	${_+_}cd ${.CURDIR}; ${MAKE} build${__target}; ${MAKE} install${__target}
.endif
.endfor

.endif

.if !target(install)
.if !target(beforeinstall)
beforeinstall:
.endif
.if !target(afterinstall)
afterinstall:
.endif
install: beforeinstall realinstall afterinstall
.ORDER: beforeinstall realinstall afterinstall
.endif

.endif
