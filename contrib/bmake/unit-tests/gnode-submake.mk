# $NetBSD: gnode-submake.mk,v 1.1 2020/11/07 23:25:06 rillig Exp $
#
# Test whether OP_SUBMAKE is determined correctly.  If it is, this node's
# shell commands are connected to the make process via pipes, to coordinate
# the number of running jobs.
#
# Determining whether a node is a sub-make node happens when the node is
# parsed.  This information is only used in parallel mode, but the result
# from parsing is available in compat mode as well.

.MAKEFLAGS: -n -dg1

all: makeinfo make-index
all: braces-dot braces-no-dot
all: braces-no-dot-modifier
all: parentheses-dot parentheses-no-dot

makeinfo:
	# The command contains the substring "make", but not as a whole word.
	: makeinfo submake

make-index:
	# The command contains the word "make", therefore it is considered a
	# possible sub-make.  It isn't really, but that doesn't hurt.
	: make-index

braces-dot:
	: ${.MAKE}

braces-no-dot:
	: ${MAKE}

braces-no-dot-modifier:
	# The command refers to MAKE, but not in its pure form.  Therefore it
	# is not considered a sub-make.
	: ${MAKE:T}

parentheses-dot:
	: $(.MAKE)

parentheses-no-dot:
	: $(MAKE)
