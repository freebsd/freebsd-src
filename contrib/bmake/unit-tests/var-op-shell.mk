# $NetBSD: var-op-shell.mk,v 1.6 2022/01/10 20:32:29 rillig Exp $
#
# Tests for the != variable assignment operator, which runs its right-hand
# side through the shell.

# The variable OUTPUT gets the output from running the shell command.
OUTPUT!=	echo "success"'ful'
.if ${OUTPUT} != "successful"
.  error
.endif

# Since 2014-08-20, the output of the shell command may be empty.
#
# On 1996-05-29, when the '!=' assignment operator and Cmd_Exec were added,
# an empty output produced the error message "Couldn't read shell's output
# for \"%s\"".
#
# The error message is still in Cmd_Exec but reserved for technical errors.
# It may be possible to trigger the error message by killing the shell after
# reading part of its output.
OUTPUT!=	true
.if ${OUTPUT} != ""
.  error
.endif

# The output of a shell command that failed is processed nevertheless.
# Unlike the other places that run external commands (expression modifier
# '::!=', expression modifier ':!...!'), a failed command generates only a
# warning, not an "error".  These "errors" are ignored in default mode, for
# compatibility, but not in lint mode (-dL).
OUTPUT!=	echo "failed"; false
.if ${OUTPUT} != "failed"
.  error
.endif

# A command with empty output may fail as well.
OUTPUT!=	false
.if ${OUTPUT} != ""
.  error
.endif

# In the output of the command, each newline is replaced with a space.
# Except for the very last one, which is discarded.
OUTPUT!=	echo "line 1"; echo "line 2"
.if ${OUTPUT} != "line 1 line 2"
.  error
.endif

# A failing command in the middle results in the exit status 0, which in the
# end means that the whole sequence of commands succeeded.
OUTPUT!=	echo "before"; false; echo "after"
.if ${OUTPUT} != "before after"
.  error
.endif

# This should result in a warning about "exited on a signal".
# This used to be kill -14 (SIGALRM), but that stopped working on
# Darwin18 after recent update.
OUTPUT!=	kill $$$$
.if ${OUTPUT} != ""
.  error
.endif

# A nonexistent command produces a non-zero exit status.
OUTPUT!=	/bin/no/such/command
.if ${OUTPUT} != ""
.  error
.endif

# The output from the shell's stderr is not captured, it just passes through.
OUTPUT!=	echo "stdout"; echo "stderr" 1>&2
.if ${OUTPUT} != "stdout"
.  error
.endif

# The 8 dollar signs end up as 4 dollar signs when expanded.  The shell sees
# the command "echo '$$$$'".  The 4 dollar signs are stored in OUTPUT, and
# when that variable is expanded, they expand to 2 dollar signs.
OUTPUT!=	echo '$$$$$$$$'
.if ${OUTPUT} != "\$\$"
.  error
.endif


# As a debugging aid, log the exact command that is run via the shell.
.MAKEFLAGS: -dv
OUTPUT!=	echo '$$$$$$$$'
.MAKEFLAGS: -d0

all:
