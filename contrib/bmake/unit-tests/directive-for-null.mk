# $NetBSD: directive-for-null.mk,v 1.4 2024/04/01 12:26:02 rillig Exp $
#
# Test for parsing a .for loop that accidentally contains a null byte.
#
# expect: make: "(stdin)" line 2: Zero byte read from file

all: .PHONY
	@printf '%s\n' \
	    '.for i in 1 2 3' \
	    'VAR=value' \
	    '.endfor' \
	| tr 'l' '\0' \
	| ${MAKE} -f -
