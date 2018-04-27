#
# $Id: elftoolchain.components.mk 3607 2018-04-13 19:41:17Z jkoshy $
#

# Knobs to turn parts of the source tree on or off.
#
# These knobs should be set to one of "yes" or "no".

# Build additional tutorial documentation. (Manual page generation is
# controlled by the 'MKDOC' knob).
WITH_ADDITIONAL_DOCUMENTATION?=yes

# Build the automation tools.
WITH_BUILD_TOOLS?=	no

# Build the instruction set analyser.
WITH_ISA?=	no

# Build PE support.
WITH_PE?=	yes

# Build test suites.
.if defined(MAKEOBJDIR) || defined(MAKEOBJDIRPREFIX)
.if defined(WITH_TESTS) && ${WITH_TESTS} == "yes"
.error Only in-tree builds are supported for tests currently [#271].
.endif
WITH_TESTS?=	no
.else
WITH_TESTS?=	yes
.endif

# Fail the build with an informative message if the value of any
# build knob is not a "yes" or "no".
.if ${WITH_ADDITIONAL_DOCUMENTATION} != "yes" && \
    ${WITH_ADDITIONAL_DOCUMENTATION} != "no"
.error Unrecognized value for WITH_ADDITIONAL_DOCUMENTATION:\
       "${WITH_ADDITIONAL_DOCUMENTATION}".
.endif
.if ${WITH_BUILD_TOOLS} != "yes" && ${WITH_BUILD_TOOLS} != "no"
.error Unrecognized value for WITH_BUILD_TOOLS: "${WITH_BUILD_TOOLS}".
.endif
.if ${WITH_ISA} != "yes" && ${WITH_ISA} != "no"
.error Unrecognized value for WITH_ISA: "${WITH_ISA}".
.endif
.if ${WITH_PE} != "yes" && ${WITH_PE} != "no"
.error Unrecognized value for WITH_PE: "${WITH_PE}".
.endif
.if ${WITH_TESTS} != "yes" && ${WITH_TESTS} != "no"
.error Unrecognized value for WITH_TESTS: "${WITH_TESTS}".
.endif
