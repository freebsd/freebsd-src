# $NetBSD: shell-csh.mk,v 1.10 2025/06/05 21:56:54 rillig Exp $
#
# Tests for using a C shell for running the commands.

CSH!=	which csh 2> /dev/null || true

# The shell path must be an absolute path.
# This is only obvious in parallel mode since in compat mode,
# simple commands are executed via execvp directly.
.if ${CSH} != ""
.SHELL: name="csh" path="${CSH}"
.endif

# In parallel mode, the shell->noPrint command is filtered from
# the output, rather naively (in PrintOutput).
#
# Until 2020-10-03, the output in parallel mode was garbled because
# the definition of the csh had been wrong since 1993 at least.
.MAKEFLAGS: -j1

all:
.if ${CSH} != ""
	# This command is both printed and executed.
	echo normal

	# This command is only executed.
	@echo hidden

	# This command is both printed and executed.
	+echo always

	# This command is both printed and executed.
	-echo ignore errors

	# In the C shell, "unset verbose" is set as the noPrint command.
	# Therefore, it is filtered from the output, rather naively.
# FIXME: Don't assume a newline character in PrintFilteredOutput.
# expect: They chatted in the sy.
	@echo 'They chatted in the sunset verbosely.'
.else
	@sed '$$d' ${MAKEFILE:.mk=.exp}	# This is cheated.
.endif
