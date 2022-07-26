# $NetBSD: opt-jobs-no-action.mk,v 1.10 2022/05/08 06:51:27 rillig Exp $
#
# Tests for the combination of the options -j and -n, which prints the
# commands instead of actually running them.
#
# The format of the output differs from the output of only the -n option,
# without the -j.  This is because all this code is implemented twice, once
# in compat.c and once in job.c.
#
# See also:
#	opt-jobs.mk
#		The corresponding tests without the -n option
#	opt-no-action-combined.mk
#		The corresponding tests without the -j option

.MAKEFLAGS: -j1 -n

# Change the templates for running the commands in jobs mode, to make it
# easier to see what actually happens.
#
# The shell attributes are handled by Job_ParseShell.
# The shell attributes 'quiet' and 'echo' don't need a trailing newline,
# this is handled by the [0] != '\0' checks in Job_ParseShell.
# The '\#' is handled by ParseRawLine.
# The '\n' is handled by Str_Words in Job_ParseShell.
# The '$$' is handled by Var_Subst in ParseDependencyLine.
.SHELL: \
	name=sh \
	path=${.SHELL} \
	quiet="\# .echoOff" \
	echo="\# .echoOn" \
	filter="\# .noPrint\n" \
	check="\# .echoTmpl\n""echo \"%s\"\n" \
	ignore="\# .runIgnTmpl\n""%s\n" \
	errout="\# .runChkTmpl\n""{ %s \n} || exit $$?\n"

all: explained combined
.ORDER: explained combined

# Explain the most basic cases in detail.
explained: .PHONY
	@+echo hide-from-output 'begin explain'

	# The following command is regular, it is printed twice:
	# - first using the template shell.echoTmpl,
	# - then using the template shell.runChkTmpl.
	false regular

	# The following command is silent, it is printed once, using the
	# template shell.runChkTmpl.
	@: silent

	# The following command ignores errors, it is printed once, using
	# the default template for cmdTemplate, which is "%s\n".
	# XXX: Why is it not printed using shell.echoTmpl as well?
	# XXX: The '-' should not influence the echoing of the command.
	-false ignore-errors

	# The following command ignores the -n command line option, it is
	# not handled by the Job module but by the Compat module, see the
	# '!silent' in Compat_RunCommand.
	+echo run despite the -n option

	@+echo hide-from-output 'end explain'
	@+echo hide-from-output


# Test all combinations of the 3 RunFlags.
#
# TODO: Closely inspect the output whether it makes sense.
# XXX: silent=no always=no ignerr={no,yes} should be almost the same.
#
SILENT.no=	# none
SILENT.yes=	@
ALWAYS.no=	# none
ALWAYS.yes=	+
IGNERR.no=	echo running
IGNERR.yes=	-echo running; false
#
combined: combined-begin

combined-begin: .PHONY
	@+echo hide-from-output 'begin combined'
	@+echo hide-from-output

.for silent in no yes
.  for always in no yes
.    for ignerr in no yes
.      for target in combined-silent-${silent}-always-${always}-ignerr-${ignerr}
combined: .WAIT ${target} .WAIT
${target}: .PHONY
	@+echo hide-from-output silent=${silent} always=${always} ignerr=${ignerr}
	${SILENT.${silent}}${ALWAYS.${always}}${IGNERR.${ignerr}}
	@+echo hide-from-output
.      endfor
.    endfor
.  endfor
.endfor

combined: combined-end
combined-end: .PHONY
	@+echo hide-from-output 'end combined'
