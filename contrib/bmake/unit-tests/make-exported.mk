# $NetBSD: make-exported.mk,v 1.7 2022/09/09 18:36:15 sjg Exp $
#
# As of 2020-08-09, the code in Var_Export is shared between the .export
# directive and the .MAKE.EXPORTED variable.  This leads to non-obvious
# behavior for certain variable assignments.

-env=		make-exported-value-env
-literal=	make-exported-value-literal
UT_VAR=		${UNEXPANDED}

# Before 2020-10-03, the following line took the code path of .export-env,
# which was surprising behavior.  Since 2020-10-03 this line tries to
# export the variable named "-env", but that is rejected because the
# variable name starts with a hyphen.
.MAKE.EXPORTED=		-env

# Before 2020-10-03, if the value of .MAKE.EXPORTED started with "-literal",
# make behaved like a mixture of .export-literal and a regular .export.
#
# Since 2020-10-03, the "variable" named "-literal" is not exported anymore,
# it is just ignored since its name starts with '-'.
.MAKE.EXPORTED=		-literal UT_VAR

all:
	@env | sort | ${EGREP} '^UT_|make-exported-value' || true
