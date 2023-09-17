# $NetBSD: opt-ignore.mk,v 1.5 2020/11/09 20:50:56 rillig Exp $
#
# Tests for the -i command line option, which ignores the exit status of the
# shell commands, and just continues with the next command, even from the same
# target.
#
# Is there a situation in which this option is useful?
#
# Why are the "Error code" lines all collected at the bottom of the output
# file, where they cannot be related to the individual shell commands that
# failed?

.MAKEFLAGS: -d0			# switch stdout to being line-buffered
.MAKEFLAGS: -i

all: dependency other

dependency:
	@echo dependency 1
	@false
	@echo dependency 2
	@:; exit 7
	@echo dependency 3

other:
	@echo other 1
	@false
	@echo other 2

all:
	@echo main 1
	@false
	@echo main 2
