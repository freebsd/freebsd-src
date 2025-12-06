# $Id: prlist.mk,v 1.7 2025/08/09 22:42:24 sjg Exp $
#
#	@(#) Copyright (c) 2006, Simon J. Gerraty
#
#	SPDX-License-Identifier: BSD-2-Clause
#
#	Please send copies of changes and bug-fixes to:
#	sjg@crufty.net
#

.if !target(__${.PARSEFILE}__)
__${.PARSEFILE}__: .NOTMAIN

# this needs to be included after all the lists it will process
# are defined - which is why it is a separate file.
# Usage looks like:
#   MAKEFLAGS= ${.MAKE} -f ${MAKEFILE} prlist.SOMETHING_HUGE | xargs whatever
#
.if make(prlist.*)
.for t in ${.TARGETS:Mprlist.*:E}
.if empty($t)
prlist.$t:
.else
prlist.$t:	${$t:O:u:S,^,prlist-,}
${$t:O:u:S,^,prlist-,}: .PHONY
	@echo "${.TARGET:S,prlist-,,}"
.endif
.endfor
.endif

.endif
