# $Id: cc-wrap.mk,v 1.5 2022/02/07 19:02:55 sjg Exp $
#
#	@(#) Copyright (c) 2022, Simon J. Gerraty
#
#	This file is provided in the hope that it will
#	be of use.  There is absolutely NO WARRANTY.
#	Permission to copy, redistribute or otherwise
#	use this file is hereby granted provided that
#	the above copyright notice and this notice are
#	left intact.
#
#	Please send copies of changes and bug-fixes to:
#	sjg@crufty.net
#

.if ${MAKE_VERSION} >= 20220126
# which targets are we interested in?
CC_WRAP_TARGETS ?= ${OBJS:U} ${POBJS:U} ${SOBJS:U}

.if !empty(CC_WRAP_TARGETS)
# cleanup
# all the target assignments below are effectively := anyway
# so we might as well do this once
CC_WRAP_TARGETS := ${CC_WRAP_TARGETS:O:u}

# what do we wrap?
CC_WRAP_LIST += CC CXX
CC_WRAP_LIST := ${CC_WRAP_LIST:O:u}

# what might we wrap them with?
CC_WRAPPERS += ccache distcc icecc
CC_WRAPPERS := ${CC_WRAPPERS:O:u}
# $W can be as simple or complicated as you like (default is just $w)
# eg.
# CCACHE ?= ${CCACHE_ENV_VARS:@v@$v='${$v}'@} ${CCACHE_CMD} ${CCACHE_FLAGS}
# or if you want global vars to be used modifiable after this include:
# CCACHE ?= $${CCACHE_ENV_VARS:@v@$$v='$${$$v}'@} $${CCACHE_CMD} $${CCACHE_FLAGS}
.for w in ${CC_WRAPPERS}
${w:tu} ?= $w
.endfor

# we do not want to make all these targets out-of-date
# just because one of the above wrappers are enabled/disabled
${CC_WRAP_TARGETS}: .MAKE.META.CMP_FILTER = ${CC_WRAPPERS:tu@W@${$W}@:S,^,N,}

# some object src types we should not wrap
CC_WRAP_SKIP_EXTS += s

# We add the sequence we care about - excluding CC_WRAP_SKIP_EXTS
# but prior filters can apply to full value of .IMPSRC
CC_WRAP_FILTER += E:tl:${CC_WRAP_SKIP_EXTS:${M_ListToSkip}}
CC_WRAP_FILTER := ${CC_WRAP_FILTER:ts:}

# last one enabled wins!
.for W in ${CC_WRAPPERS:tu}
.if ${MK_$W:U} == "yes"
.for C in ${CC_WRAP_LIST}
# we have to protect the check of .IMPSRC from Global expansion
${CC_WRAP_TARGETS}: $C = $${"$${.IMPSRC:${CC_WRAP_FILTER}}":?${$W}:} ${$C}
.endfor
.endif
.endfor

.endif
.endif

