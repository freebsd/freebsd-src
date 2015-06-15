# $FreeBSD$

# Note: This file is also duplicated in the sys/conf/kern.pre.mk so
# it will always grab SRCCONF, even if it isn't being built in-tree
# to preserve historical (and useful) behavior. Changes here need to
# be reflected there so SRCCONF isn't included multiple times.

# make sure this is defined in a consistent manner
SRCTOP:= ${.PARSEDIR:tA:H:H}

# Allow user to configure things that only effect src tree builds.
SRCCONF?=	/etc/src.conf
.if (exists(${SRCCONF}) || ${SRCCONF} != "/etc/src.conf") && !target(_srcconf_included_)
.sinclude "${SRCCONF}"
_srcconf_included_:	.NOTMAIN
.endif
# If we were found via .../share/mk we need to replace that
# with ${.PARSEDIR:tA} so that we can be found by
# sub-makes launched from objdir.
.if ${.MAKEFLAGS:M.../share/mk} != ""
.MAKEFLAGS:= ${.MAKEFLAGS:S,.../share/mk,${.PARSEDIR:tA},}
.endif
.if ${MAKESYSPATH:Uno:M*.../*} != ""
MAKESYSPATH:= ${MAKESYSPATH:S,.../share/mk,${.PARSEDIR:tA},}
.export MAKESYSPATH
.endif
# tempting, but bsd.compiler.mk causes problems this early
#.include "src.opts.mk"
