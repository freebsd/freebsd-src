# $NetBSD: cond-func-make-main.mk,v 1.1 2020/11/22 19:37:27 rillig Exp $
#
# Test how accurately the make() function in .if conditions reflects
# what is actually made.
#
# There are several ways to specify what is being made:
#
# 1. The default main target is the first target in the given makefiles that
#    is not one of the special targets.  For example, .PHONY is special when
#    it appears on the left-hand side of the ':'.  It is not special on the
#    right-hand side though.
#
# 2. Command line arguments that are neither options (-ds or -k) nor variable
#    assignments (VAR=value) are interpreted as targets to be made.  These
#    override the default main target from above.
#
# 3. All sources of the first '.MAIN: sources' line.  Any further .MAIN line
#    is treated as if .MAIN were a regular name.
#
# This test only covers items 1 and 3.  For item 2, see cond-func-make.mk.

first-main-target:
	: Making ${.TARGET}.

# Even though the main-target would actually be made at this point, it is
# ignored by the make() function.
.if make(first-main-target)
.  error
.endif

# Declaring a target via the .MAIN dependency adds it to the targets to be
# created (opts.create), but only that list was empty at the beginning of
# the line.  This implies that several main targets can be set at the name
# time, but they have to be in the same dependency group.
#
# See ParseDoDependencyTargetSpecial, branch SP_MAIN.
.MAIN: dot-main-target-1a dot-main-target-1b

.if !make(dot-main-target-1a)
.  error
.endif
.if !make(dot-main-target-1b)
.  error
.endif

dot-main-target-{1,2}{a,b}:
	: Making ${.TARGET}.

# At this point, the list of targets to be made (opts.create) is not empty
# anymore.  ParseDoDependencyTargetSpecial therefore treats the .MAIN as if
# it were an ordinary target.  Since .MAIN is not listed as a dependency
# anywhere, it is not made.
.if target(.MAIN)
.  error
.endif
.MAIN: dot-main-target-2a dot-main-target-2b
.if !target(.MAIN)
.  error
.endif
.if make(dot-main-target-2a)
.  error
.endif
