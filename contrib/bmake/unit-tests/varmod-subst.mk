# $NetBSD: varmod-subst.mk,v 1.17 2025/03/29 19:08:53 rillig Exp $
#
# Tests for the :S,from,to, variable modifier.

all: mod-subst
all: mod-subst-delimiter
all: mod-subst-chain
all: mod-subst-dollar

WORDS=		sequences of letters

# The empty pattern never matches anything, except if it is anchored at the
# beginning or the end of the word.
.if ${WORDS:S,,,} != ${WORDS}
.  error
.endif

# The :S modifier flag '1' is applied exactly once.
.if ${WORDS:S,e,*,1} != "s*quences of letters"
.  error
.endif

# The :S modifier flag '1' is applied to the first occurrence, no matter if
# the occurrence is in the first word or not.
.if ${WORDS:S,f,*,1} != "sequences o* letters"
.  error
.endif

# The :S modifier replaces every first match per word.
.if ${WORDS:S,e,*,} != "s*quences of l*tters"
.  error
.endif

# The :S modifier flag 'g' replaces every occurrence.
.if ${WORDS:S,e,*,g} != "s*qu*nc*s of l*tt*rs"
.  error
.endif

# The '^' in the search pattern anchors the pattern at the beginning of each
# word, thereby matching a prefix.
.if ${WORDS:S,^sequ,occurr,} != "occurrences of letters"
.  error
.endif

# The :S modifier with a '^' anchor replaces the whole word if that word is
# exactly the pattern.
.if ${WORDS:S,^of,with,} != "sequences with letters"
.  error
.endif

# The :S modifier does not match if the pattern is longer than the word.
.if ${WORDS:S,^office,does not match,} != ${WORDS}
.  warning
.endif

# The '$' in the search pattern anchors the pattern at the end of each word,
# thereby matching a suffix.
.if ${WORDS:S,f$,r,} != "sequences or letters"
.  error
.endif

# The :S modifier with a '$' anchor replaces at most one occurrence per word.
.if ${WORDS:S,s$,,} != "sequence of letter"
.  error
.endif

# The :S modifier with a '$' anchor replaces the whole word if that word is
# exactly the pattern.
.if ${WORDS:S,of$,,} != "sequences letters"
.  error
.endif

# The :S modifier with a '$' anchor and a pattern that is longer than a word
# cannot match that word.
.if ${WORDS:S,eof$,,} != ${WORDS}
.  warning
.endif

# The :S modifier with the '^' and '$' anchors matches an exact word.
.if ${WORDS:S,^of$,,} != "sequences letters"
.  error
.endif

# The :S modifier with the '^' and '$' anchors does not match a word that
# starts with the pattern but is longer than the pattern.
.if ${WORDS:S,^o$,,} != ${WORDS}
.  error
.endif

# The :S modifier with the '^' and '$' anchors does not match a word that ends
# with the pattern but is longer than the pattern.
.if ${WORDS:S,^f$,,} != ${WORDS}
.  error
.endif

# The :S modifier with the '^' and '$' anchors does not match a word if the
# pattern ends with the word but is longer than the word.
.if ${WORDS:S,^eof$,,} != ${WORDS}
.  error
.endif

# The :S modifier with the '^' and '$' anchors does not match a word if the
# pattern starts with the word but is longer than the word.
.if ${WORDS:S,^office$,,} != ${WORDS}
.  error
.endif

# Except for the '^' and '$' anchors, the pattern does not contain any special
# characters, so the '*' from the pattern would only match a literal '*' in a
# word.
.if ${WORDS:S,*,replacement,} != ${WORDS}
.  error
.endif

# Except for the '^' and '$' anchors, the pattern does not contain any special
# characters, so the '.' from the pattern would only match a literal '.' in a
# word.
.if ${WORDS:S,.,replacement,} != ${WORDS}
.  error
.endif

# The '&' in the replacement is a placeholder for the text matched by the
# pattern.
.if ${:Uvalue:S,^val,&,} != "value"
.  error
.endif
.if ${:Uvalue:S,ue$,&,} != "value"
.  error
.endif
.if ${:Uvalue:S,^val,&-&-&,} != "val-val-value"
.  error
.endif
.if ${:Uvalue:S,ue$,&-&-&,} != "value-ue-ue"
.  error
.endif


# When a word is replaced with nothing, the remaining words are separated by a
# single space, not two.
.if ${1 2 3:L:S,2,,} != "1 3"
.  error
.endif


# In an empty expression, the ':S' modifier matches a single time, but only if
# the search string is empty and anchored at either the beginning or the end
# of the word.
.if ${:U:S,,out-of-nothing,} != ""
.  error
.endif
.if ${:U:S,^,out-of-nothing,} != "out-of-nothing"
.  error
.endif
.if ${:U:S,$,out-of-nothing,} != "out-of-nothing"
.  error
.endif
.if ${:U:S,^$,out-of-nothing,} != "out-of-nothing"
.  error
.endif
.if ${:U:S,,out-of-nothing,g} != ""
.  error
.endif
.if ${:U:S,^,out-of-nothing,g} != "out-of-nothing"
.  error
.endif
.if ${:U:S,$,out-of-nothing,g} != "out-of-nothing"
.  error
.endif
.if ${:U:S,^$,out-of-nothing,g} != "out-of-nothing"
.  error
.endif
.if ${:U:S,,out-of-nothing,W} != ""
.  error
.endif
.if ${:U:S,^,out-of-nothing,W} != "out-of-nothing"
.  error
.endif
.if ${:U:S,$,out-of-nothing,W} != "out-of-nothing"
.  error
.endif
.if ${:U:S,^$,out-of-nothing,W} != "out-of-nothing"
.  error
.endif


mod-subst:
	@echo $@:
	@echo :${:Ua b b c:S,a b,,:Q}:
	@echo :${:Ua b b c:S,a b,,1:Q}:
	@echo :${:Ua b b c:S,a b,,W:Q}:
	@echo :${:Ua b b c:S,b,,g:Q}:
	@echo :${:U1 2 3 1 2 3:S,1 2,___,Wg:S,_,x,:Q}:
	@echo ${:U12345:S,,sep,g:Q}

# The :S and :C modifiers accept an arbitrary character as the delimiter,
# including characters that are otherwise used as escape characters or
# interpreted in a special way.  This can be used to confuse humans.
mod-subst-delimiter:
	@echo $@:
	@echo ${:U1 2 3:S	2	two	:Q} horizontal tabulator
	@echo ${:U1 2 3:S 2 two :Q} space
	@echo ${:U1 2 3:S!2!two!:Q} exclamation mark
	@echo ${:U1 2 3:S"2"two":Q} quotation mark
	# In shell command lines, the hash does not need to be escaped.
	# It needs to be escaped in variable assignment lines though.
	@echo ${:U1 2 3:S#2#two#:Q} number sign
	@echo ${:U1 2 3:S$2$two$:Q} dollar sign
	@echo ${:U1 2 3:S%2%two%:Q} percent sign
	@echo ${:U1 2 3:S&2&two&:Q} ampersand
	@echo ${:U1 2 3:S'2'two':Q} apostrophe
	@echo ${:U1 2 3:S(2(two(:Q} left parenthesis
	@echo ${:U1 2 3:S)2)two):Q} right parenthesis
	@echo ${:U1 2 3:S*2*two*:Q} asterisk
	@echo ${:U1 2 3:S+2+two+:Q} plus sign
	@echo ${:U1 2 3:S,2,two,:Q} comma
	@echo ${:U1 2 3:S-2-two-:Q} hyphen-minus
	@echo ${:U1 2 3:S.2.two.:Q} full stop
	@echo ${:U1 2 3:S/2/two/:Q} solidus
	@echo ${:U1 2 3:S121two1:Q} digit
	@echo ${:U1 2 3:S:2:two::Q} colon
	@echo ${:U1 2 3:S;2;two;:Q} semicolon
	@echo ${:U1 2 3:S<2<two<:Q} less-than sign
	@echo ${:U1 2 3:S=2=two=:Q} equals sign
	@echo ${:U1 2 3:S>2>two>:Q} greater-than sign
	@echo ${:U1 2 3:S?2?two?:Q} question mark
	@echo ${:U1 2 3:S@2@two@:Q} commercial at
	@echo ${:U1 2 3:SA2AtwoA:Q} capital letter
	@echo ${:U1 2 3:S[2[two[:Q} left square bracket
	@echo ${:U1 2 3:S\2\two\:Q} reverse solidus
	@echo ${:U1 2 3:S]2]two]:Q} right square bracket
	@echo ${:U1 2 3:S^2^two^:Q} circumflex accent
	@echo ${:U1 2 3:S_2_two_:Q} low line
	@echo ${:U1 2 3:S`2`two`:Q} grave accent
	@echo ${:U1 2 3:Sa2atwoa:Q} small letter
	@echo ${:U1 2 3:S{2{two{:Q} left curly bracket
	@echo ${:U1 2 3:S|2|two|:Q} vertical line
	@echo ${:U1 2 3:S}2}two}:Q} right curly bracket
	@echo ${:U1 2 3:S~2~two~:Q} tilde

# The :S and :C modifiers can be chained without a separating ':'.
# This is not documented in the manual page.
# It works because ApplyModifier_Subst scans for the known modifiers g1W
# and then just returns to ApplyModifiers.  There, the colon is optionally
# skipped (see the *st.next == ':' at the end of the loop).
#
# Most other modifiers cannot be chained since their parsers skip until
# the next ':' or '}' or ')'.
mod-subst-chain:
	@echo $@:
	@echo ${:Ua b c:S,a,A,S,b,B,}.
	# There is no 'i' modifier for the :S or :C modifiers.
	# The error message is "make: Unknown modifier 'i'", which is
	# kind of correct, although it is mixing the terms for variable
	# modifiers with the matching modifiers.
# expect: make: Unknown modifier ":i"
	@echo ${:Uvalue:S,a,x,i}.

# No matter how many dollar signs there are, they all get merged
# into a single dollar by the :S modifier.
#
# As of 2020-08-09, this is because ParseModifierPart sees a '$' and
# calls Var_Parse to expand the variable.  In all other places, the "$$"
# is handled outside of Var_Parse.  Var_Parse therefore considers "$$"
# one of the "really stupid names", skips the first dollar, and parsing
# continues with the next character.  This repeats for the other dollar
# signs, except the one before the delimiter.  That one is handled by
# the code that optionally interprets the '$' as the end-anchor in the
# first part of the :S modifier.  That code doesn't call Var_Parse but
# simply copies the dollar to the result.
mod-subst-dollar:
	@echo $@:${:U1:S,^,$,:Q}:
	@echo $@:${:U2:S,^,$$,:Q}:
	@echo $@:${:U3:S,^,$$$,:Q}:
	@echo $@:${:U4:S,^,$$$$,:Q}:
	@echo $@:${:U5:S,^,$$$$$,:Q}:
	@echo $@:${:U6:S,^,$$$$$$,:Q}:
	@echo $@:${:U7:S,^,$$$$$$$,:Q}:
	@echo $@:${:U8:S,^,$$$$$$$$,:Q}:
	@echo $@:${:U40:S,^,$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$,:Q}:
# This generates no dollar at all:
	@echo $@:${:UU8:S,^,${:U$$$$$$$$},:Q}:
# Here is an alternative way to generate dollar signs.
# It's unexpectedly complicated though.
	@echo $@:${:U:range=5:ts\x24:C,[0-9],,g:Q}:
# In modifiers, dollars are escaped using the backslash, not using another
# dollar sign.  Therefore, creating a dollar sign is pretty simple:
	@echo $@:${:Ugood3:S,^,\$\$\$,:Q}
