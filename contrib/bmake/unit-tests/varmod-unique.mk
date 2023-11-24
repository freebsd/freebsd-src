# $NetBSD: varmod-unique.mk,v 1.6 2021/12/05 22:37:58 rillig Exp $
#
# Tests for the :u variable modifier, which discards adjacent duplicate
# words.

.if ${1 2 1:L:u} != "1 2 1"
.  warning The modifier ':u' only merges _adjacent_ duplicate words.
.endif

.if ${1 2 2 3:L:u} != "1 2 3"
.  warning The modifier ':u' must merge adjacent duplicate words.
.endif

.if ${:L:u} != ""
.  warning The modifier ':u' must do nothing with an empty word list.
.endif

.if ${   :L:u} != ""
.  warning The modifier ':u' must normalize the whitespace.
.endif

.if ${word:L:u} != "word"
.  warning The modifier ':u' must do nothing with a single-element word list.
.endif

.if ${   word   :L:u} != "word"
.  warning The modifier ':u' must normalize the whitespace.
.endif

.if ${1 1 1 1 1 1 1 1:L:u} != "1"
.  warning The modifier ':u' must merge _all_ adjacent duplicate words.
.endif

.if ${   1    2    1 1  :L:u} != "1 2 1"
.  warning The modifier ':u' must normalize whitespace between the words.
.endif

.if ${1 1 1 1 2:L:u} != "1 2"
.  warning Duplicate words at the beginning must be merged.
.endif

.if ${1 2 2 2 2:L:u} != "1 2"
.  warning Duplicate words at the end must be merged.
.endif

all:
