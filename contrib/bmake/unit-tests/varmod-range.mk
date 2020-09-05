# $NetBSD: varmod-range.mk,v 1.3 2020/08/23 15:13:21 rillig Exp $
#
# Tests for the :range variable modifier, which generates sequences
# of integers from the given range.

all:
	@echo ${a b c:L:rang}			# modifier name too short
	@echo ${a b c:L:range}			# ok
	@echo ${a b c:L:rango}			# misspelled
	@echo ${a b c:L:ranger}			# modifier name too long
