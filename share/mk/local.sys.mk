
.if !target(_${_this}_)
_${_this}_: .NOTMAIN

.if ${MK_DIRDEPS_BUILD} == "yes" || ${MK_META_MODE} == "yes"

# Not in the below list as it may make sense for non-meta mode
# eventually.  meta.sys.mk (DIRDEPS_BUILD) also already adds these in.
.if ${MK_DIRDEPS_BUILD} == "no" && ${MK_META_MODE} == "yes"
MAKE_PRINT_VAR_ON_ERROR += \
	.ERROR_TARGET \
	.ERROR_META_FILE \
	.MAKE.LEVEL \
	MAKEFILE \
	.MAKE.MODE
.endif

_ERROR_CMD_EXEC=	${sed -n '/^CMD/s,^CMD \(.*\),\1;,p' ${.ERROR_META_FILE}:L:sh}
_ERROR_CMD=		${!empty(.ERROR_META_FILE):?${_ERROR_CMD_EXEC}:.PHONY}
MAKE_PRINT_VAR_ON_ERROR+= \
	_ERROR_CMD \
	.CURDIR \
	.MAKE \
	.OBJDIR \
	.TARGETS \
	CPUTYPE \
	DESTDIR \
	LD_LIBRARY_PATH \
	MACHINE \
	MACHINE_ARCH \
	MACHINE_CPUARCH \
	MAKEOBJDIRPREFIX \
	MAKESYSPATH \
	MAKE_VERSION \
	PATH \
	SRCTOP \
	OBJTOP \
	${MAKE_PRINT_VAR_ON_ERROR_XTRAS}

# Meta mode may rebuild targets that then fail. The next build won't detect
# the meta mode change. Not all targets have a 'rm ${.TARGET}' in them
# so force it.
.DELETE_ON_ERROR:

.if ${.MAKE.LEVEL} > 0
MAKE_PRINT_VAR_ON_ERROR += .MAKE.MAKEFILES .PATH
.endif
.endif

.if !empty(.OBJDIR)
OBJTOP?= ${.OBJDIR:S,${.CURDIR},,}${SRCTOP}
.endif

.if !empty(LIBDIR)
_PREMK_LIBDIR:=	${LIBDIR}
.endif

.include "src.sys.mk"
.-include <site.sys.mk>

# this will be set via local.meta.sys.env.mk if appropriate
MK_host_egacy?= no

.if ${.MAKE.MODE:Mmeta*} != ""
# we can afford to use cookies to prevent some targets
# re-running needlessly but only when using filemon.
# Targets that should support the meta mode cookie handling should just be
# added to META_TARGETS.  If bsd.sys.mk cannot be included then ${META_DEPS}
# should be added as a target dependency as well.  Otherwise the target
# is added to in bsd.sys.mk since it comes last.
.if ${.MAKE.MODE:Mnofilemon} == ""
# Prepend .OBJDIR if not already there.
_META_COOKIE_COND=	"${.TARGET:M${.OBJDIR}/*}" == ""
_META_COOKIE_DEFAULT=	${${_META_COOKIE_COND}:?${.OBJDIR}/${.TARGET}:${.TARGET}}
# Use the default if COOKIE.${.TARGET} is not defined.
META_COOKIE=		${COOKIE.${.TARGET}:U${_META_COOKIE_DEFAULT}}
META_COOKIE_RM=		@rm -f ${META_COOKIE}
META_COOKIE_TOUCH=	@touch ${META_COOKIE}
CLEANFILES+=		${META_TARGETS}
_meta_dep_before:	.USEBEFORE .NOTMAIN
	${META_COOKIE_RM}
_meta_dep_after:	.USE .NOTMAIN
	${META_COOKIE_TOUCH}
# Attach this to a target to allow it to benefit from meta mode's
# not rerunning a command if it doesn't need to be considering its
# metafile/filemon-tracked dependencies.
META_DEPS=	_meta_dep_before _meta_dep_after .META
.endif
.else
# some targets need to be .PHONY - but not in meta mode
META_NOPHONY=	.PHONY
.endif
META_NOPHONY?=
META_COOKIE_RM?=
META_COOKIE_TOUCH?=
META_DEPS+=	${META_NOPHONY}

.if ${MK_DIRDEPS_BUILD} == "yes"
.if ${MACHINE:Nhost*:Ncommon} != "" && ${MACHINE} != ${HOST_MACHINE}
# cross-building
CROSS_TARGET_FLAGS?= -target ${MACHINE_ARCH}-unknown-freebsd${FREEBSD_REVISION}
CFLAGS+= ${CROSS_TARGET_FLAGS}
ACFLAGS+= ${CROSS_TARGET_FLAGS}
.endif
.endif

.endif
