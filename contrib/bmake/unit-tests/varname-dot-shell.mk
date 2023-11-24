# $NetBSD: varname-dot-shell.mk,v 1.7 2021/02/04 21:42:47 rillig Exp $
#
# Tests for the special .SHELL variable, which contains the shell used for
# running the commands.
#
# This variable is read-only.

.MAKEFLAGS: -dcpv

ORIG_SHELL:=	${.SHELL}

.SHELL=		overwritten
.if ${.SHELL} != ${ORIG_SHELL}
.  error
.endif

# Trying to append to the variable.
# Since 2020-10-30 this is prevented.
.MAKEFLAGS: .SHELL+=appended
.if ${.SHELL} != ${ORIG_SHELL}
.  error
.endif

# Trying to delete the variable.
# This has no effect since the variable is not defined in the global scope,
# but in the command-line scope.
.undef .SHELL
.SHELL=		newly overwritten
.if ${.SHELL} != ${ORIG_SHELL}
.  error
.endif

.MAKEFLAGS: -d0

all:
	@:;
