# $NetBSD: varmod-hash.mk,v 1.3 2020/08/23 15:13:21 rillig Exp $
#
# Tests for the :hash variable modifier.

all:
	@echo ${12345:L:has}			# modifier name too short
	@echo ${12345:L:hash}			# ok
	@echo ${12345:L:hash=SHA-256}		# :hash does not accept '='
	@echo ${12345:L:hasX}			# misspelled
	@echo ${12345:L:hashed}			# modifier name too long
