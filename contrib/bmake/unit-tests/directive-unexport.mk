# $NetBSD: directive-unexport.mk,v 1.4 2020/10/30 23:54:42 sjg Exp $
#
# Tests for the .unexport directive.

# TODO: Implementation

# First, export 3 variables.
UT_A=	a
UT_B=	b
UT_C=	c
.export UT_A UT_B UT_C

# Show the exported variables and their values.
.info ${:!env|sort|grep '^UT_'!}
.info ${.MAKE.EXPORTED}

# XXX: Now try to unexport all of them.  The variables are still exported
# but not mentioned in .MAKE.EXPORTED anymore.
# See the ":N" in Var_UnExport for the implementation.
*=	asterisk
.unexport *

.info ${:!env|sort|grep '^UT_'!}
.info ${.MAKE.EXPORTED}

all:
	@:;
