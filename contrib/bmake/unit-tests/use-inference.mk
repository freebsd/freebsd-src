# $NetBSD: use-inference.mk,v 1.3 2020/12/07 00:53:30 rillig Exp $
#
# Demonstrate that .USE rules do not have an effect on inference rules.
# At least not in the special case where the inference rule does not
# have any associated commands.

.SUFFIXES:
.SUFFIXES: .from .to

all: use-inference.to

verbose: .USE
	@echo 'Verbosely making $@ out of $>'

.from.to: verbose
# Since this inference rule does not have any associated commands, it
# is ignored.
#
#	@echo 'Building $@ from $<'

use-inference.from:		# assume it exists
	@echo 'Building $@ from nothing'

# Possible but unproven explanation:
#
# The main target is "all", which depends on "use-inference.to".
# The inference connects the .from to the .to file, otherwise make
# would not know that the .from file would need to be built.
#
# The .from file is then built.
#
# After this, make stops since it doesn't know how to make the .to file.
# This is strange since make definitely knows about the .from.to suffix
# inference rule.  But it seems to ignore it, maybe because it doesn't
# have any associated commands.

# Until 2020-12-07, despite the error message "don't know how to make",
# the exit status was 0.  This was inconsistent.
