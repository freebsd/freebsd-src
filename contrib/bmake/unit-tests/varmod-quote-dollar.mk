# $NetBSD: varmod-quote-dollar.mk,v 1.3 2022/01/22 17:10:51 rillig Exp $
#
# Tests for the :q variable modifier, which quotes the string for the shell
# and doubles dollar signs, to prevent them from being interpreted by a
# child process of make.

ASCII_CHARS=	${.newline} !"\#$$%&'()*+,-./09:;<=>?@AZ[\]^_`az{|}~

all:
	@${MAKE} -r -f /dev/null CHARS=${ASCII_CHARS:q} -V CHARS
