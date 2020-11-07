# $NetBSD: varname-make_print_var_on_error.mk,v 1.4 2020/10/23 06:18:23 rillig Exp $
#
# Tests for the special MAKE_PRINT_VAR_ON_ERROR variable, which prints the
# values of selected variables on error.

# XXX: As of 2020-10-23, the .ERROR_CMD variable is pointless in compat mode
# since at the point where it is filled in PrintOnError, the first command in
# gn->commands has been set to NULL already.  This leaves .ERROR_CMD an empty
# list.

MAKE_PRINT_VAR_ON_ERROR=	.ERROR_TARGET .ERROR_CMD

all:
	@: command before
	@echo fail; false
	@: command after
