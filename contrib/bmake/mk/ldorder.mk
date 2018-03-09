# $Id: ldorder.mk,v 1.18 2018/02/11 18:27:59 sjg Exp $
#
#	@(#) Copyright (c) 2015, Simon J. Gerraty
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

# Try to compute optimal link order.
# When using only shared libs link order does not much matter,
# but archive libs are a different matter.

# We can construct a graph of .ldorder-lib${LIB*} dependencies
# and associate each with _LDORDER_USE to output the relevant
# ld flags.
# Due to the nature of make, the result will be in the reverse order
# that we want to feed to ld.
# So we need to reverse it before use.

.if !target(_LDORDER_USE)
# does caller want to use ldorder?
# yes for prog, normally no for lib
_ldorder_use := ${.ALLTARGETS:Mldorder}

.-include <local.ldorder.mk>

# convert /path/to/libfoo.a into _{LIBFOO}
LDORDER_INC_FILTER += S,+,PLUS,g S,.so$$,,g
LDORDER_LIBS_FILTER += O:u
LDORDER_INC ?= ldorder.inc
REFERENCE_FILE ?= :

_LDORDER_USE: .ldorder-rm .USE .NOTMAIN
	@echo depends: ${.ALLSRC:M.ldorder-lib*} > /dev/null
	@echo ${LDADD_${.TARGET:T:S,.ldorder-,,}:U${.TARGET:T:S/.ldorder-lib/-l/}} >> .ldorder
	@${META_COOKIE_TOUCH}

# we need to truncate our working file
.ldorder-rm: .NOTMAIN
	@rm -f .ldorder ldorder-*
	@${.ALLSRC:O:u:@f@${REFERENCE_FILE} < $f;@}
	@${META_COOKIE_TOUCH}

# make sure this exists
.ldorder:	.NOTMAIN

# and finally we need to reverse the order of content
ldorder: .ldorder .NOTMAIN
	@{ test ! -s .ldorder || cat -n .ldorder | sort -rn | \
	sed '/ldorder-/d;s,^[[:space:]0-9]*,,'; } > ${.TARGET}

# Initially we hook contents of DPLIBS and DPADD into our graph
LDORDER_LIBS ?= ${DPLIBS} ${DPADD:M*/lib*} ${__dpadd_libs}
# we need to remember this
_LDORDER_LIBS := ${LDORDER_LIBS:${LDORDER_LIBS_FILTER:ts:}}

.if empty(_LDORDER_LIBS)
# don't use stale ldorder
LDADD_LDORDER =
.else
# this is how you use it
LDADD_LDORDER ?= `cat ldorder`
.endif

# for debug below
_ldorder = ${RELDIR}.${TARGET_SPEC}

.endif				# !target(_LDORDER_USE)

.if !empty(LDORDER_LIBS) && !empty(_ldorder_use)
# canonicalize - these are just tokens anyway
LDORDER_LIBS := ${LDORDER_LIBS:${LDORDER_LIBS_FILTER:ts:}:R:C/\.so.*//}
_ldorders := ${LDORDER_LIBS:T:Mlib*:S,^,.ldorder-,}

.for t in ${_ldorders}
.if !target($t)
$t: _LDORDER_USE
.endif
.endfor

# and this makes it all happen
.ldorder: ${_ldorders}

# this is how we get the dependencies
.if ${.INCLUDEDFROMFILE:M*.${LDORDER_INC}} != ""
_ldorder := .ldorder-${.INCLUDEDFROMFILE:S/.${LDORDER_INC}//}
${_ldorder}: ${_ldorders}
.ldorder-rm: ${.INCLUDEDFROMDIR}/${.INCLUDEDFROMFILE}
.endif

# set DEBUG_LDORDER to pattern[s] that match the dirs of interest
.if ${DEBUG_LDORDER:Uno:@x@${RELDIR:M$x}@} != ""
.info ${_ldorder}: ${_ldorders}
.endif

# now try to find more ...
# each *.${LDORDER_INC} should set LDORDER_LIBS to what it needs
# it can also add to CFLAGS etc.
.for __inc in ${LDORDER_LIBS:S,$,.${LDORDER_INC},}
.if !target(__${__inc}__)
__${__inc}__:
# make sure this is reset
LDORDER_LIBS =
.-include <${__inc}>
.endif
.endfor

.endif				# !empty(LDORDER_LIBS)

.ifdef LIB
# you can make this depend on files (must match *ldorder*)
# to add extra content - like CFLAGS
libLDORDER_INC = lib${LIB}.${LDORDER_INC}
.if !commands(${libLDORDER_INC})
${libLDORDER_INC}:
	@(cat /dev/null ${.ALLSRC:M*ldorder*}; \
	echo 'LDORDER_LIBS= ${_LDORDER_LIBS:T:R:${LDORDER_INC_FILTER:ts:}:tu:C,.*,_{&},}'; \
	echo; echo '.include <ldorder.mk>' ) | sed 's,_{,$${,g' > ${.TARGET}
.endif
.endif
