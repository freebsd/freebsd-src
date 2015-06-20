# $FreeBSD$

# Note: This file is also duplicated in the sys/conf/kern.pre.mk so
# it will always grab SRCCONF, even if it isn't being built in-tree
# to preserve historical (and useful) behavior. Changes here need to
# be reflected there so SRCCONF isn't included multiple times.

# Allow user to configure things that only effect src tree builds.
SRCCONF?=	/etc/src.conf
.if (exists(${SRCCONF}) || ${SRCCONF} != "/etc/src.conf") && !target(_srcconf_included_)
.sinclude "${SRCCONF}"
_srcconf_included_:	.NOTMAIN
.endif

# tempting, but bsd.compiler.mk causes problems this early
# probably need to remove dependence on bsd.own.mk 
#.include "src.opts.mk"
