# $NetBSD: varname-dot-make-mode.mk,v 1.3 2022/05/07 17:49:47 rillig Exp $
#
# Tests for the special .MAKE.MODE variable.

# TODO: test .MAKE.MODE "meta", or see meta mode tests.
# TODO: test .MAKE.MODE "compat"


# See Makefile, POSTPROC for the postprocessing that takes place.
# See the .rawout file for the raw output before stripping the digits.
all: .PHONY make-mode-randomize-targets


# By adding the word "randomize-targets" to the variable .MAKE.MODE, the
# targets are not made in declaration order, but rather in random order.  This
# mode helps to find undeclared dependencies between files.
#
# History
#	Added on 2022-05-07.
#
# See also
#	https://gnats.netbsd.org/45226
make-mode-randomize-targets: .PHONY
	@echo "randomize compat mode:"
	@${MAKE} -r -f ${MAKEFILE} randomize-targets

	@echo "randomize jobs mode (-j1):"
	@${MAKE} -r -f ${MAKEFILE} -j1 randomize-targets

	@echo "randomize jobs mode (-j5):"
	@${MAKE} -r -f ${MAKEFILE} -j5 randomize-targets | grep '^:'

.if make(randomize-targets)
randomize-targets: .WAIT a1 a2 a3 .WAIT b1 b2 b3 .WAIT c1 c2 c3 .WAIT
a1 a2 a3 b1 b2 b3 c1 c2 c3:
	: Making ${.TARGET}

# .MAKE.MODE is evaluated after parsing all files, so it suffices to switch
# the mode after defining the targets.
.MAKE.MODE+=	randomize-targets
.endif
