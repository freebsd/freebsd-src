# $NetBSD: impsrc.mk,v 1.3 2020/08/07 13:43:50 rillig Exp $

# Does ${.IMPSRC} work properly?
# It should be set, in order of precedence, to ${.TARGET} of:
#  1) the implied source of a transformation rule,
#  2) the first prerequisite from the dependency line of an explicit rule, or
#  3) the first prerequisite of an explicit rule.
#
# Items 2 and 3 work in GNU make.
# Items 2 and 3 are not required by POSIX 2018.

all: target1.z target2 target3 target4

.SUFFIXES: .x .y .z

.x.y: source1
	@echo 'expected: target1.x'
	@echo 'actual:   $<'

.y.z: source2
	@echo 'expected: target1.y'
	@echo 'actual:   $<'

# (3) Making target1.z out of target1.y is done because of an inference rule.
# Therefore $< is available here.

# (2) This is an additional dependency on the inference rule .x.y.
# The dependency target1.x comes from the inference rule,
# therefore it is available as $<.
target1.y: source3

# (1) This is an explicit dependency, not an inference rule.
# Therefore POSIX does not specify that $< be available here.
target1.x: source4
	@echo 'expected: '		# either 'source4' or ''
	@echo 'actual:   $<'

# (4) This is an explicit dependency, independent of any inference rule.
# Therefore $< is not available here.
target2: source1 source2
	@echo 'expected: '
	@echo 'actual:   $<'

# (5) These are two explicit dependency rules.
# The first doesn't have any dependencies, only the second has.
# If any, the value of $< would be 'source2'.
target3: source1
target3: source2 source3
	@echo 'expected: '
	@echo 'actual:   $<'

# (6) The explicit rule does not have associated commands.
# The value of $< might come from that rule,
# but it's equally fine to leave $< undefined.
target4: source1
target4:
	@echo 'expected: '
	@echo 'actual:   $<'

source1 source2 source3 source4:
