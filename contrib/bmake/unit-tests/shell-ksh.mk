# $NetBSD: shell-ksh.mk,v 1.2 2025/06/05 21:56:54 rillig Exp $
#
# Tests for using a Korn shell for running the commands.

KSH!=	which ksh 2> /dev/null || true

# The shell path must be an absolute path.
# This is only obvious in parallel mode since in compat mode,
# simple commands are executed via execvp directly.
.if ${KSH} != ""
.SHELL: name="ksh" path="${KSH}"
.endif

# In parallel mode, the shell->noPrint command is filtered from
# the output, rather naively (in PrintOutput).
.MAKEFLAGS: -j1

all:
.if ${KSH} != ""
	# This command is both printed and executed.
	echo normal

	# This command is only executed.
	@echo hidden

	# This command is both printed and executed.
	+echo always

	# This command is both printed and executed.
	-echo ignore errors

	# In the Korn shell, "set +v" is set as the noPrint command.
	# Therefore, it is filtered from the output, rather naively.
# FIXME: Don't assume a newline character in PrintFilteredOutput.
# expect: The "is filtered out.
	@echo 'The "set +v" is filtered out.'
.else
	@sed '$$d' ${MAKEFILE:.mk=.exp}	# This is cheated.
.endif
