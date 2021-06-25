# $NetBSD: varmod-unique.mk,v 1.5 2021/05/30 20:26:41 rillig Exp $
#
# Tests for the :u variable modifier, which discards adjacent duplicate
# words.

.if ${:U1 2 1:u} != "1 2 1"
.  warning The :u modifier only merges _adjacent_ duplicate words.
.endif

.if ${:U1 2 2 3:u} != "1 2 3"
.  warning The :u modifier must merge adjacent duplicate words.
.endif

.if ${:U:u} != ""
.  warning The :u modifier must do nothing with an empty word list.
.endif

.if ${:U   :u} != ""
.  warning The modifier ':u' must normalize the whitespace.
.endif

.if ${:Uword:u} != "word"
.  warning The :u modifier must do nothing with a single-element word list.
.endif

.if ${:U   word   :u} != "word"
.  warning The modifier ':u' must normalize the whitespace.
.endif

.if ${:U1 1 1 1 1 1 1 1:u} != "1"
.  warning The :u modifier must merge _all_ adjacent duplicate words.
.endif

.if ${:U   1    2    1 1  :u} != "1 2 1"
.  warning The :u modifier must normalize whitespace between the words.
.endif

.if ${:U1 1 1 1 2:u} != "1 2"
.  warning Duplicate words at the beginning must be merged.
.endif

.if ${:U1 2 2 2 2:u} != "1 2"
.  warning Duplicate words at the end must be merged.
.endif

all:
	@:;
