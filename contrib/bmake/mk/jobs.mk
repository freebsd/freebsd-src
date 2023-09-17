# $Id: jobs.mk,v 1.9 2023/04/27 18:10:27 sjg Exp $
#
#	@(#) Copyright (c) 2012-2023, Simon J. Gerraty
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

# This makefile is used by top-level makefile.
# With the following:
#
#	.if make(*-jobs)
#	.include <jobs.mk>
#	.endif
#
# 
# Then if you do:
#
#	mk target-jobs
#
# We will run:
#
#	${MAKE} -j${JOB_MAX} target > ${JOB_LOGDIR}/target.log 2>&1
#
# JOB_MAX defaults to 8 but should normally be derrived based on the
# number of cpus available.  The wrapper script 'mk' makes that easy.
#

now_utc ?= ${%s:L:gmtime}
.if !defined(start_utc)
start_utc := ${now_utc}
.endif

.if make(*-jobs)
.info ${.newline}${TIME_STAMP} Start ${.TARGETS}

JOB_LOGDIR ?= ${SRCTOP:H}
JOB_LOG = ${JOB_LOGDIR}/${.TARGET:S,-jobs,,:S,/,_,g}.log
JOB_LOG_GENS ?= 4
# we like to rotate logs
.if empty(NEWLOG_SH)
.for d in ${.SYSPATH:U${.PARSEDIR}:@x@$x $x/scripts@}
.if exists($d/newlog.sh)
NEWLOG_SH := $d/newlog.sh
.if ${MAKE_VERSION} > 20220924
.break
.endif
.endif
.endfor
.if empty(NEWLOG_SH)
.ifdef M_whence
NEWLOG_SH := ${newlog.sh:L:${M_whence}}
.else
NEWLOG_SH := ${(type newlog.sh) 2> /dev/null:L:sh:M/*}
.endif
.endif
.endif
.if !empty(NEWLOG_SH) && exists(${NEWLOG_SH})
NEWLOG := sh ${NEWLOG_SH}
JOB_NEWLOG_ARGS ?= -S -n ${JOB_LOG_GENS}
.else
NEWLOG = :
.endif

.if ${.MAKE.JOBS:U0} > 0
JOB_MAX= ${.MAKE.JOBS}
.else
# This should be derrived from number of cpu's
JOB_MAX?= 8
JOB_ARGS+= -j${JOB_MAX}
.endif

# we need to reset .MAKE.LEVEL to 0 do that
# build orchestration works as expected (DIRDEPS_BUILD)
${.TARGETS:M*-jobs}:
	@${NEWLOG} ${JOB_NEWLOG_ARGS} ${JOB_LOG}
	@echo "${TIME_STAMP} Start ${.TARGET:S,-jobs,,} ${JOB_ARGS} ${JOB_LOG_START} log=${JOB_LOG}" | tee ${JOB_LOG}
	@cd ${.CURDIR} && env MAKELEVEL=0 \
	${.MAKE} ${JOB_ARGS} _TARGETS=${.TARGET:S,-jobs,,} ${.TARGET:S,-jobs,,} >> ${JOB_LOG} 2>&1

.endif

.END: _build_finish
.ERROR: _build_failed

_build_finish:  .NOMETA
	@echo "${TIME_STAMP} Finished ${.TARGETS} seconds=`expr ${now_utc} - ${start_utc}`"

_build_failed: .NOMETA
	@echo "${TIME_STAMP} Failed ${.TARGETS} seconds=`expr ${now_utc} - ${start_utc}`"
