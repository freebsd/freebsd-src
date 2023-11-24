# $NetBSD: varmod-to-one-word.mk,v 1.3 2020/12/20 23:29:50 rillig Exp $
#
# Tests for the :tW variable modifier, which treats the variable value
# as a single word, for all following modifiers.

SENTENCE=	The quick brown fox jumps over the lazy brown dog.

.if ${SENTENCE:tW:[#]} != 1
.  error
.endif
.if ${SENTENCE:tw:[#]} != 10
.  error
.endif

# Protect against accidental freeing of the variable value.
.if ${SENTENCE} != "The quick brown fox jumps over the lazy brown dog."
.  error
.endif

all:
	@:;
