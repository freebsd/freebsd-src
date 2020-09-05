# $NetBSD: make-exported.mk,v 1.1 2020/08/09 12:59:16 rillig Exp $
#
# As of 2020-08-09, the code in Var_Export is shared between the .export
# directive and the .MAKE.EXPORTED variable.  This leads to non-obvious
# behavior for certain variable assignments.

-env=		make-exported-value
-literal=	make-exported-value
UT_VAR=		${UNEXPANDED}

# The following behavior is probably not intended.
.MAKE.EXPORTED=		-env		# like .export-env
.MAKE.EXPORTED=		-literal UT_VAR	# like .export-literal PATH

all:
	@env | sort | grep -E '^UT_|make-exported-value' || true
