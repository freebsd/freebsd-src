# $NetBSD: opt-keep-going.mk,v 1.4 2020/10/18 18:12:42 rillig Exp $
#
# Tests for the -k command line option, which stops building a target as soon
# as an error is detected, but continues building the other, independent
# targets, as far as possible.

.MAKEFLAGS: -d0			# switch stdout to being line-buffered

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
