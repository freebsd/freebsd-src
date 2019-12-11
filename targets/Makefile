# $FreeBSD$

# This is the top-level makefile - derived from the Junos version
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
DIRDEPS_FILTER = Mtargets/*
.endif


.if !empty(build_options)
build_options := ${build_options:O:u}
.for v in ${build_options}
$v = yes
.endfor
.export ${build_options}
.endif

# this does the work
.include <dirdeps-targets.mk>

.if !empty(DIRDEPS)
# This is printed as we read the makefile
# so provides a useful clue as to when we really started.
# This allows us to work out how long reading 
# Makefile.depend* takes.
.if ${.MAKEFLAGS:M-V} == ""
.if ${BUILD_DIRDEPS_CACHE:Uno} == "no"
.info ${.newline}${TIME_STAMP} Start ${.TARGETS}
.endif
now_utc = ${%s:L:gmtime}
start_utc := ${now_utc}
.endif

_begin = 
.if ${BUILD_DIRDEPS_CACHE:Uno} == "no"
__DEFAULT_YES_OPTIONS+= \
	CLEAN_ERROR_LOGS

.include <bsd.mkopt.mk>

.if ${MK_CLEAN_ERROR_LOGS} == "yes"
_begin += clean-error-logs
.endif

.if !empty(_begin) && !make(clean*)
dirdeps: ${_begin} .WAIT
.endif
.endif

.include "Makefile.inc"

.include <dirdeps.mk>

.for t in ${.TARGETS:Nall:Nclean*:${_begin:Uall:${M_ListToSkip}}}
$t: dirdeps
.endfor

elapsed_time= seconds=`expr ${now_utc} - ${start_utc}`

.if ${BUILD_DIRDEPS_CACHE:Uno} == "no"
.END: _build_finish
_build_finish:	.NOMETA
	@echo "${TIME_STAMP} Finished ${.TARGETS} ${elapsed_time}"
.endif

.ERROR: _build_failed
_build_failed: .NOMETA
	@echo "${TIME_STAMP} Failed ${.TARGETS} ${elapsed_time}"

.endif					# !empty(DIRDEPS)

clean-error-logs: .NOMETA
	@test ! -d ${meta_error_log:H} || rm -f ${meta_error_log:H}/*log

.if !target(_DIRDEP_USE)
# we did not read dirdeps.mk above, the target may be here
.include "Makefile.xtras"
.endif

.else
# dirdeps does it all
all:
.endif					# .MAKE.LEVEL == 0

