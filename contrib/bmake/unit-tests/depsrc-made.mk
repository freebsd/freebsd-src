# $NetBSD: depsrc-made.mk,v 1.3 2020/09/05 15:57:12 rillig Exp $
#
# Tests for the special source .MADE in dependency declarations,
# which marks all its dependencies as already made, so their commands
# don't need to be executed.
#
# TODO: Describe a possible use case for .MADE.

all: part1 part2

part1: chapter11 chapter12 .MADE
part2: chapter21 chapter22

chapter11 chapter12 chapter21 chapter22:
	: Making ${.TARGET}
