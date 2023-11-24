# $NetBSD: directive-for-null.mk,v 1.3 2022/06/12 15:03:27 rillig Exp $
#
# Test for parsing a .for loop that accidentally contains a null byte.
#
# As of 2020-12-19, there are 3 error messages:
#
#	make: "(stdin)" line 2: Zero byte read from file
#	make: "(stdin)" line 2: Unexpected end of file in for loop.
#	make: "(stdin)" line 3: Zero byte read from file
#
# The one about "end of file" might be misleading but is due to the
# implementation.  On both errors and EOF, ParseRawLine returns NULL.
#
# The one about the "zero byte" in line 3 is surprising since the only
# line that contains a null byte is line 2.

all: .PHONY
	@printf '%s\n' \
	    '.for i in 1 2 3' \
	    'VAR=value' \
	    '.endfor' \
	| tr 'l' '\0' \
	| ${MAKE} -f -
