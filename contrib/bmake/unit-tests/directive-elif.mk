# $NetBSD: directive-elif.mk,v 1.8 2023/06/01 20:56:35 rillig Exp $
#
# Tests for the .elif directive.
#
# Misspellings of the .elif directive are not always detected.  They are only
# detected if the conditional branch directly above it is taken.  In all other
# cases, make skips over the skipped branch as fast as possible, looking only
# at the initial '.' of the line and whether the directive is one of the known
# conditional directives.  All other directives are silently ignored, as they
# could be variable assignments or dependency declarations as well, and
# deciding this would cost time.


# TODO: Implementation


# Misspelling '.elsif' below an .if branch that is not taken.
.if 0
.  info This branch is not taken.
# As of 2020-12-19, the misspelling is not recognized as a conditional
# directive and is thus silently skipped.
#
# Since the .if condition evaluated to false, this whole branch is not taken.
.elsif 0
.  info XXX: This misspelling is not detected.
.  info This branch is not taken.
# Even if the misspelling were detected, the branch would not be taken
# since the condition of the '.elsif' evaluates to false as well.
.endif


# Misspelling '.elsif' below an .if branch that is not taken.
.if 0
.  info This branch is not taken.
# As of 2020-12-19, the misspelling is not recognized as a conditional
# directive and is thus silently skipped.  Since the .if condition evaluated
# to false, this whole branch is not taken.
.elsif 1
.  info XXX: This misspelling is not detected.
# If the misspelling were detected, this branch would be taken.
.endif


# Misspelling '.elsif' below an .if branch that is taken.
.if 1
# This misspelling is in an active branch and is therefore detected.
# expect+1: Unknown directive "elsif"
.elsif 0
# The only thing that make detects here is a misspelled directive, make
# doesn't recognize that it was meant to be a conditional directive.
# Therefore the branch continues here, even though the '.elsif' condition
# evaluates to false.
# expect+1: This branch is taken.
.  info This branch is taken.
.endif


# Misspelling '.elsif' below an .if branch that is taken.
.if 1
# The misspelling is in an active branch and is therefore detected.
# expect+1: Unknown directive "elsif"
.elsif 1
# Since both conditions evaluate to true, this branch is taken no matter
# whether make detects a misspelling or not.
# expect+1: This branch is taken.
.  info This branch is taken.
.endif


# Misspelling '.elsif' in a skipped branch below a branch that was taken.
.if 1
# expect+1: This branch is taken.
.  info This branch is taken.
.elif 0
.  info This branch is not taken.
.elsif 1
.  info XXX: This misspelling is not detected.
.endif


# Misspelling '.elsif' in an .else branch that is not taken.
.if 1
.else
.  info This branch is not taken.
.elsif 1
.  info XXX: This misspelling is not detected.
.endif


# Misspelling '.elsif' in an .else branch that is taken.
.if 0
.else
# expect+1: Unknown directive "elsif"
.elsif 1
# expect+1: This misspelling is detected.
.  info This misspelling is detected.
# expect+1: This branch is taken because of the .else.
.  info This branch is taken because of the .else.
.endif


# Misspellings for .elif in a .elif branch that is not taken.
.if 0
.  info This branch is not taken.
.elif 0				# ok
.  info This branch is not taken.
.elsif 0
.  info XXX: This misspelling is not detected.
.  info This branch is not taken.
.elseif 0
.  info XXX: This misspelling is not detected.
.  info This branch is not taken.
.endif


# expect+1: What happens on misspelling in a skipped branch?
.info What happens on misspelling in a skipped branch?
.if 0
.  info 0-then
.elsif 1
.  info XXX: This misspelling is not detected.
.  info 1-elsif
.elsif 2
.  info XXX: This misspelling is not detected.
.  info 2-elsif
.else
# expect+1: else
.  info else
.endif

# expect+1: What happens on misspelling in a taken branch?
.info What happens on misspelling in a taken branch?
.if 1
# expect+1: 1-then
.  info 1-then
# expect+1: Unknown directive "elsif"
.elsif 1
# expect+1: 1-elsif
.  info 1-elsif
# expect+1: Unknown directive "elsif"
.elsif 2
# expect+1: 2-elsif
.  info 2-elsif
.else
.  info else
.endif

# expect+1: if-less elif
.elif 0

.if 1
.else
# expect+1: warning: extra elif
.elif
.endif

all:
