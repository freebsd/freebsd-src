# $NetBSD: varname-dot-shell.mk,v 1.2 2020/08/23 09:28:52 rillig Exp $
#
# Tests for the special .SHELL variable, which contains the shell used for
# running the commands.
#
# This variable is read-only.

ORIG_SHELL:=	${.SHELL}

.SHELL=		overwritten
.if ${.SHELL} != ${ORIG_SHELL}
.error
.endif

# Trying to delete the variable.
# This has no effect since the variable is not defined in the global context,
# but in the command-line context.
.undef .SHELL
.SHELL=		newly overwritten
.if ${.SHELL} != ${ORIG_SHELL}
.error
.endif

all:
	@echo ${.SHELL:M*}
