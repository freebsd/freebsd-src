# $NetBSD: opt-ignore.mk,v 1.3 2020/08/23 14:28:04 rillig Exp $
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
