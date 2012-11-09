# $FreeBSD$

# This is the top-level makefile - derrived from the Junos version
#
# If a subdir that matches the requested target exists, we assume
# a build target and initialize DIRDEPS, dirdeps.mk does the rest.
#
# Otherwise we include Makefile.xtras and hope it knows what to do.
#

# Copyright (c) 2010-2012, Juniper Networks, Inc.
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions 
# are met: 
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer. 
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.  
# 
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 

.if ${.MAKE.LEVEL} == 0

# this is our top-level makefile
.if make(pkg-*)
DIRDEPS_FILTER = Mpkgs/*
.endif

# in theory, this is what we want
target_dirs = pkgs pkgs/pseudo
# these tweak how we do it
target_prefix = pkg- build-
DIRDEPS := ${.TARGETS:Nall:${target_prefix:@p@S,^$p,,@:ts:}:@t@${target_dirs:@d@$d/$t@}@:@d@${exists(${SRCTOP}/$d):?$d:}@}

.if ${DIRDEPS:Mpkgs/pseudo/*} != ""
# all bets are off
PKG_MACHINE_LIST = ${ALL_MACHINE_LIST}
.endif

.if make(check-commit)
# a special case
DIRDEPS = pkgs/pseudo/check-commit
.if defined(ALL_MACHINES)
CHECK_MACHINE_LIST = all
.undef ALL_MACHINES
.endif
SHIPDIR = no

.else

.if defined(ALL_MACHINES)
DIRDEPS := ${DIRDEPS:O:u:@d@${ALL_MACHINE_LIST:O:u:@m@${exists(${SRCTOP}/$d/${.MAKE.DEPENDFILE:T:R}.$m):?$d.$m:}@}@}
.undef ALL_MACHINES
PKG_MACHINE_LIST ?= ${DIRDEPS:E:O:u}
.elif defined(HOST_MACHINE) && ${MACHINE} == ${HOST_MACHINE}
# the above may be insufficient.
# some packages only support one machine which may not be ${MACHINE}
# some support multiple, in which case unless ALL_MACHINES is defined
# we only want ${MACHINE}
plain := ${DIRDEPS:@d@${exists(${SRCTOP}/$d/${.MAKE.DEPENDFILE:T:R}):?$d:}@}
.if ${plain} != ${DIRDEPS}
all_machine_list = ${ALL_MACHINE_LIST} host
qual := ${DIRDEPS:${plain:${M_ListToSkip}}:@d@${all_machine_list:O:u:@m@${exists(${SRCTOP}/$d/${.MAKE.DEPENDFILE:T:R}.$m):?$d.$m:}@}@}
DIRDEPS := ${plain} ${qual}
PKG_MACHINE_LIST ?= ${MACHINE} ${qual:E:O:u}
.endif
.else
# check that a .MAKE.DEPENDFILE exists
DIRDEPS := ${.MAKE.DEPENDFILE_PREFERENCE:T:@m@${DIRDEPS:@d@${exists(${SRCTOP}/$d/$m):?$d:}@}@:O:u}
.endif
.if !empty(PKG_MACHINE_LIST)
.if ${PKG_MACHINE_LIST:Mdepend} != ""
PKG_MACHINE_LIST = ${ALL_MACHINE_LIST}
.endif
PKG_MACHINE_LIST := ${PKG_MACHINE_LIST}
.endif
.endif

.if !empty(build_options)
build_options := ${build_options:O:u}
.for v in ${build_options}
$v = yes
.endfor
.export ${build_options}
.endif

.if !empty(DIRDEPS)
# This is printed as we read the makefile
# so provides a useful clue as to when we really started.
# This allows us to work out how long reading 
# Makefile.depend* takes.
.if ${.MAKEFLAGS:M-V} == ""
.info ${TIME_STAMP} Start ${.TARGETS}
now_utc = ${%s:L:gmtime}
start_utc := ${now_utc}
.endif

_begin = count-makefiles

# by default, we clean pkgs/ which can otherwise accumulate a lot of cruft
.if ${NEED_CLEAN_PKGS:Uyes:tl} != "no"
_begin += clean-pkgs
.endif
.if defined(SHIPDIR) && ${NEED_CLEAN_SHIPDIR:Uno:tl} != "no" && exists(${SHIPDIR})
_begin += clean-shipdir
.endif
.if ${NEED_CLEAN_ERROR_LOGS:Uyes:tl} == "yes"
_begin += clean-error-logs
.endif
.if !empty(_begin) && !make(clean*)
dirdeps: ${_begin} .WAIT
.endif

.include "Makefile.inc"

.include <dirdeps.mk>

.for t in ${.TARGETS:Nall:Nclean*:${_begin:Uall:${M_ListToSkip}}}
$t: dirdeps
.endfor

count-makefiles: .NOMETA
	@echo "${TIME_STAMP} Makefiles read: total=${.MAKE.MAKEFILES:[#]} depend=${.MAKE.MAKEFILES:M*depend*:[#]} seconds=`expr ${now_utc} - ${start_utc}`"

.END: _build_finish
_build_finish:	.NOMETA
	@echo "${TIME_STAMP} Finished ${.TARGETS}"

.ERROR: _build_failed
_build_failed: .NOMETA
	@echo "${TIME_STAMP} Failed ${.TARGETS}"

.endif					# !empty(DIRDEPS)

clean-error-logs: .NOMETA
	@test ! -d ${meta_error_log:H} || rm -f ${meta_error_log:H}/*log

clean-pkgs:	.NOMETA
	@rm -rf ${ALL_MACHINE_LIST:N${MACHINE}:@m@${OBJROOT}$m/pkgs@} ${OBJROOT}${MACHINE}/pkgs/*

clean-shipdir:	.NOMETA
	${"${SHIPDIR:tl:Nno:N*\:*}":?rm -f ${SHIPDIR}/*:}

.if !target(_DIRDEP_USE)
# we did not read dirdeps.mk above, the target may be here
.include "Makefile.xtras"
.endif

.else
# dirdeps does it all
all:
.endif					# .MAKE.LEVEL == 0

