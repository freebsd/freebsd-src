# $NetBSD: varmod-quote-dollar.mk,v 1.4 2022/05/08 10:14:40 rillig Exp $
#
# Tests for the :q variable modifier, which quotes the string for the shell
# and doubles dollar signs, to prevent them from being interpreted by a
# child process of make.

# The newline and space characters at the beginning of this string are passed
# to the child make.  When the child make parses the variable assignment, it
# discards the leading space characters.
ASCII_CHARS=	${.newline} !"\#$$%&'()*+,-./09:;<=>?@AZ[\]^_`az{|}~

all:
	@${MAKE} -r -f /dev/null \
	    CHARS=${ASCII_CHARS:q} \
	    TWICE=${ASCII_CHARS:q}${ASCII_CHARS:q} \
	    -V CHARS \
	    -V TWICE
