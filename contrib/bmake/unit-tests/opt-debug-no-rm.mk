# $NetBSD: opt-debug-no-rm.mk,v 1.1 2020/09/05 06:20:51 rillig Exp $
#
# Tests for the -dn command line option, which prevents the temporary
# command scripts from being removed from the temporary directory.

# TODO: Implementation

# TODO: Does this apply to non-jobs mode?
# TODO: Does this apply to jobs mode?
# TODO: Are the generated filenames predictable?

all:
	@:;
