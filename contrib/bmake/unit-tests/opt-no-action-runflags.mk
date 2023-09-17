# $NetBSD: opt-no-action-runflags.mk,v 1.1 2020/12/09 07:57:52 rillig Exp $
#
# Tests for the -n command line option, which runs almost no commands,
# combined with the RunFlags '@', '-', '+' for individual commands.
#
# See also:
#	opt-jobs-no-action.mk
#		The corresponding test with the -j option

.MAKEFLAGS: -n

all: .PHONY combined

SILENT.no=	# none
SILENT.yes=	@
ALWAYS.no=	# none
ALWAYS.yes=	+
IGNERR.no=	echo running
IGNERR.yes=	-echo running; false
#
combined: .PHONY
	@+echo hide-from-output 'begin $@'; echo
.for silent in no yes
.  for always in no yes
.    for ignerr in no yes
	@+echo hide-from-output silent=${silent} always=${always} ignerr=${ignerr}
	${SILENT.${silent}}${ALWAYS.${always}}${IGNERR.${ignerr}}
	@+echo hide-from-output
.    endfor
.  endfor
.endfor
	@+echo hide-from-output 'end $@'
