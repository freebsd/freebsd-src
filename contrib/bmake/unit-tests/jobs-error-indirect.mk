# $NetBSD: jobs-error-indirect.mk,v 1.1 2020/12/01 17:50:04 rillig Exp $
#
# Ensure that in jobs mode, when a command fails, the current directory is
# printed, to aid in debugging.
#
# XXX: This test is run without the -k flag, which prints "stopped in" twice.
# Why?
#
# This particular case is not the cause for the PRs, but it is very close.
#
# https://gnats.netbsd.org/55578
# https://gnats.netbsd.org/55832
#
#

.MAKEFLAGS: -j1

all: .PHONY indirect

indirect: .PHONY
	false
