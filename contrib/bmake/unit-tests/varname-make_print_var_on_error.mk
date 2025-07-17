# $NetBSD: varname-make_print_var_on_error.mk,v 1.6 2021/02/04 21:33:14 rillig Exp $
#
# Tests for the special MAKE_PRINT_VAR_ON_ERROR variable, which prints the
# values of selected variables on error.

# XXX: As of 2020-10-23, the .ERROR_CMD variable is pointless in compat mode
# since at the point where it is filled in PrintOnError, the first command in
# gn->commands has been set to NULL already.  This leaves .ERROR_CMD an empty
# list.
#
# See also:
#	compat-error.mk

# XXX: As of 2021-02-04, PrintOnError calls Var_Subst with SCOPE_GLOBAL, which
# does not expand the node-local variables like .TARGET.  This results in the
# double '${.TARGET}' in the output.

MAKE_PRINT_VAR_ON_ERROR=	.ERROR_TARGET .ERROR_CMD

all:
	@: before '${.TARGET}' '$${.TARGET}' '$$$${.TARGET}'
	echo fail ${.TARGET}; false '${.TARGET}' '$${.TARGET}' '$$$${.TARGET}'
	@: after '${.TARGET}' '$${.TARGET}' '$$$${.TARGET}'
