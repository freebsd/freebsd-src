#
# $Id: elftoolchain.components.mk 3316 2016-01-11 22:48:22Z jkoshy $
#

# Knobs to turn parts of the source tree on or off.

# Build the automation tools.
WITH_BUILD_TOOLS=	no

# Build additional tutorial documentation. (Manual page generation is
# controlled by the 'MKDOC' knob).
WITH_DOCUMENTATION=yes

# Build the instruction set analyser.
WITH_ISA=	no

# Build PE support.
WITH_PE=	yes

# Build test suites.
WITH_TESTS=	yes
