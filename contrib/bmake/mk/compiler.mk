# SPDX-License-Identifier: BSD-2-Clause
#
# $Id: compiler.mk,v 1.14 2024/02/19 00:05:50 sjg Exp $
#
#	@(#) Copyright (c) 2019, Simon J. Gerraty
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

.if !target(__${.PARSEFILE}__)
__${.PARSEFILE}__: .NOTMAIN

.if ${MACHINE} == "common"
COMPILER_TYPE = none
COMPILER_VERSION = 0
.endif
.if empty(COMPILER_TYPE) || empty(COMPILER_VERSION)
# gcc does not always say gcc
_v != (${CC} --version) 2> /dev/null | \
	${EGREP:Uegrep} -i 'clang|cc|[1-9]\.[0-9]|Free Software Foundation'; echo
.if empty(COMPILER_TYPE)
.if ${_v:Mclang} != ""
COMPILER_TYPE = clang
.elif ${_v:M[Gg][Cc][Cc]} != "" || ${_v:MFoundation*} != "" || ${CC:Ucc:T:M*gcc*} != ""
COMPILER_TYPE = gcc
.endif
.endif
.if empty(COMPILER_VERSION)
COMPILER_VERSION != echo "${_v:M[1-9][0-9]*.[0-9]*}:[1]" | \
	awk -F. '{print $$1 * 10000 + $$2 * 100 + $$3;}'
.endif
.undef _v
.endif
# just in case we don't recognize compiler
COMPILER_TYPE ?= unknown
COMPILER_VERSION ?= 0
.endif
