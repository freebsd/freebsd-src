
# Note: This file is also duplicated in the sys/conf/kern.pre.mk so
# it will always grab SRCCONF, even if it isn't being built in-tree
# to preserve historical (and useful) behavior. Changes here need to
# be reflected there so SRCCONF isn't included multiple times.

.if !defined(_WITHOUT_SRCCONF)
# Allow user to configure things that only effect src tree builds.
.if exists(${SRCTOP}/src.conf)
SRCCONF?=	${SRCTOP}/src.conf
.else
SRCCONF?=	/etc/src.conf
.endif
.if !empty(SRCCONF) && \
    (exists(${SRCCONF}) || ${SRCCONF} != "/etc/src.conf") && \
    !target(_srcconf_included_)

# Validate that the user didn't try setting an env-only variable in
# their src.conf. This benefits from already including bsd.mkopt.mk.
.for var in ${__ENV_ONLY_OPTIONS:O:u}
__presrcconf_${var}:=	${MK_${var}:U-}${WITHOUT_${var}:Uno:Dyes}${WITH_${var}:Uno:Dyes}
.endfor

.sinclude "${SRCCONF}"
_srcconf_included_:	.NOTMAIN

# Validate the env-only variables.
.for var in ${__ENV_ONLY_OPTIONS:O:u}
__postrcconf_${var}:=	${MK_${var}:U-}${WITHOUT_${var}:Uno:Dyes}${WITH_${var}:Uno:Dyes}
.if ${__presrcconf_${var}} != ${__postrcconf_${var}}
.error Option ${var} may only be defined in ${SRC_ENV_CONF}, environment, or make argument, not ${SRCCONF}.
.endif
.undef __presrcconf_${var}
.undef __postrcconf_${var}
.endfor

.endif # SRCCONF
.endif

# The following should be removed no earlier than LLVM11 being imported into the
# tree, to ensure we don't regress the build.  LLVM11 and GCC10 will switch the
# default over to -fno-common, making this redundant.
CFCOMMONFLAG?=	-fno-common
CFLAGS+=	${CFCOMMONFLAG}
.if defined(PACKAGE_BUILDING)
CFLAGS+=	-fmacro-prefix-map=${SRCTOP}=/usr/src -fdebug-prefix-map=${SRCTOP}=/usr/src
.endif

DEFAULTWARNS?=	6

# tempting, but bsd.compiler.mk causes problems this early
# probably need to remove dependence on bsd.own.mk 
#.include "src.opts.mk"
