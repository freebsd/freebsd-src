# $NetBSD: sh-flags.mk,v 1.4 2020/12/12 12:19:18 rillig Exp $
#
# Tests for the effective RunFlags of a shell command (run/skip, echo/silent,
# error check, trace), which are controlled by 12 different switches.  These
# switches interact in various non-obvious ways.  To analyze the interactions,
# this test runs each possible combination of these switches, for now.
#
# As soon as an interaction of switches is identified as obvious and expected,
# this particular interaction may be removed from the test, to focus on the
# remaining ones.
#
# See also:
#	Compat_RunCommand
#	JobPrintSpecials

all: .PHONY

opt-ignerr.yes=		-i
opt-jobs.yes=		-j1
opt-loud.no=		-d0	# side effect: make stdout unbuffered
opt-loud.yes=		-dl	# side effect: make stdout unbuffered
opt-no-action.yes=	-n
opt-silent.yes=		-s
opt-xtrace.yes=		-dx
tgt-always.yes=		.MAKE
tgt-ignerr.yes=		.IGNORE
tgt-silent.yes=		.SILENT
cmd-always.yes=		+
cmd-ignerr.yes=		-
cmd-silent.yes=		@

letter.always.yes=	a
letter.ignerr.yes=	i
letter.jobs.yes=	j
letter.loud.yes=	l
letter.no-action.yes=	n
letter.silent.yes=	s
letter.xtrace.yes=	x

.if !defined(OPT_TARGET)
.for opt-ignerr in no yes
.for opt-jobs in no yes
.for opt-loud in no yes
.for opt-no-action in no yes
# Only 'no', not 'yes', since job->echo is based trivially on opts.silent.
.for opt-silent in no
# Only 'no', not 'yes', since that would add uncontrollable output from
# reading /etc/profile or similar files.
.for opt-xtrace in no

target=		opt-
target+=	${letter.ignerr.${opt-ignerr}:U_}
target+=	${letter.jobs.${opt-jobs}:U_}
target+=	${letter.loud.${opt-loud}:U_}
target+=	${letter.no-action.${opt-no-action}:U_}
target+=	${letter.silent.${opt-silent}:U_}
target+=	${letter.xtrace.${opt-xtrace}:U_}

.for target in ${target:ts}

MAKE_CMD.${target}=	${MAKE}
MAKE_CMD.${target}+=	${opt-ignerr.${opt-ignerr}}
MAKE_CMD.${target}+=	${opt-jobs.${opt-jobs}}
MAKE_CMD.${target}+=	${opt-loud.${opt-loud}}
MAKE_CMD.${target}+=	${opt-no-action.${opt-no-action}}
MAKE_CMD.${target}+=	${opt-silent.${opt-silent}}
MAKE_CMD.${target}+=	${opt-xtrace.${opt-xtrace}}
MAKE_CMD.${target}+=	-f ${MAKEFILE}
MAKE_CMD.${target}+=	OPT_TARGET=${target}
MAKE_CMD.${target}+=	${target}

all: ${target}
${target}: .PHONY
	@${MAKE_CMD.${target}:M*}

.endfor
.endfor
.endfor
.endfor
.endfor
.endfor
.endfor
.endif

SILENT.yes=	@
ALWAYS.yes=	+
IGNERR.yes=	-

.if defined(OPT_TARGET)
.for tgt-always in no yes
.for tgt-ignerr in no yes
.for tgt-silent in no yes
.for cmd-always in no yes
.for cmd-ignerr in no yes
.for cmd-silent in no yes

target=		${OPT_TARGET}-tgt-
target+=	${letter.always.${tgt-always}:U_}
target+=	${letter.ignerr.${tgt-ignerr}:U_}
target+=	${letter.silent.${tgt-silent}:U_}
target+=	-cmd-
target+=	${letter.always.${cmd-always}:U_}
target+=	${letter.ignerr.${cmd-ignerr}:U_}
target+=	${letter.silent.${cmd-silent}:U_}

.for target in ${target:ts}

${OPT_TARGET}: .WAIT ${target} .WAIT
.if ${tgt-always} == yes
${target}: .MAKE
.endif
.if ${tgt-ignerr} == yes
${target}: .IGNORE
.endif
.if ${tgt-silent} == yes
${target}: .SILENT
.endif

RUNFLAGS.${target}=	${SILENT.${cmd-silent}}${ALWAYS.${cmd-always}}${IGNERR.${cmd-ignerr}}
.if ${OPT_TARGET:M*i*} || ${tgt-ignerr} == yes || ${cmd-ignerr} == yes
CMD.${target}=		echo running; false
.else
CMD.${target}=		echo running
.endif

${target}: .PHONY
	@+echo hide-from-output
	@+echo hide-from-output ${target}
	${RUNFLAGS.${target}} ${CMD.${target}}
.endfor

.endfor
.endfor
.endfor
.endfor
.endfor
.endfor
.endif
