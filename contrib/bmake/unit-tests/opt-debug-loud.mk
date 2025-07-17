# $NetBSD: opt-debug-loud.mk,v 1.5 2023/12/19 19:33:40 rillig Exp $
#
# Tests for the -dl command line option, which prints the commands before
# running them, ignoring the command line option for silent mode (-s) as
# well as the .SILENT special source and target, as well as the '@' prefix
# for shell commands.

.MAKEFLAGS: -dl -s
.SILENT:

# The -dl command line option does not affect commands that are run when
# evaluating expressions and their modifiers, such as :!cmd! or :sh.
.if ${:!echo word!} != "word"
.  error
.endif

all: .SILENT
	# Even though the command line option -s is given, .SILENT is set
	# for all targets and for this target in particular, the command
	# is still printed.  The -dl debugging option is stronger than all
	# of these.
	@echo all-word
