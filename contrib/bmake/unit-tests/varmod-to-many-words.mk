# $NetBSD: varmod-to-many-words.mk,v 1.3 2020/12/20 23:29:50 rillig Exp $
#
# Tests for the :tw modifier, which treats the variable as many words,
# to undo a previous :tW modifier.

SENTENCE=	The quick brown fox jumps over the lazy brown dog.

.if ${SENTENCE:tw:[#]} != 10
.  error
.endif
.if ${SENTENCE:tW:[#]} != 1
.  error
.endif

# Protect against accidental freeing of the variable value.
.if ${SENTENCE} != "The quick brown fox jumps over the lazy brown dog."
.  error
.endif

all:
	@:;
