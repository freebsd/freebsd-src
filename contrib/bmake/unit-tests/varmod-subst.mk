# $NetBSD: varmod-subst.mk,v 1.4 2020/10/24 08:46:08 rillig Exp $
#
# Tests for the :S,from,to, variable modifier.

all: mod-subst
all: mod-subst-delimiter
all: mod-subst-chain
all: mod-subst-dollar

WORDS=		sequences of letters
.if ${WORDS:S,,,} != ${WORDS}
.  warning The empty pattern matches something.
.endif
.if ${WORDS:S,e,*,1} != "s*quences of letters"
.  warning The :S modifier flag '1' is not applied exactly once.
.endif
.if ${WORDS:S,f,*,1} != "sequences o* letters"
.  warning The :S modifier flag '1' is only applied to the first word,\
	 not to the first occurrence.
.endif
.if ${WORDS:S,e,*,} != "s*quences of l*tters"
.  warning The :S modifier does not replace every first match per word.
.endif
.if ${WORDS:S,e,*,g} != "s*qu*nc*s of l*tt*rs"
.  warning The :S modifier flag 'g' does not replace every occurrence.
.endif
.if ${WORDS:S,^sequ,occurr,} != "occurrences of letters"
.  warning The :S modifier fails for a short match anchored at the start.
.endif
.if ${WORDS:S,^of,with,} != "sequences with letters"
.  warning The :S modifier fails for an exact match anchored at the start.
.endif
.if ${WORDS:S,^office,does not match,} != ${WORDS}
.  warning The :S modifier matches a too long pattern anchored at the start.
.endif
.if ${WORDS:S,f$,r,} != "sequences or letters"
.  warning The :S modifier fails for a short match anchored at the end.
.endif
.if ${WORDS:S,s$,,} != "sequence of letter"
.  warning The :S modifier fails to replace one occurrence per word.
.endif
.if ${WORDS:S,of$,,} != "sequences letters"
.  warning The :S modifier fails for an exact match anchored at the end.
.endif
.if ${WORDS:S,eof$,,} != ${WORDS}
.  warning The :S modifier matches a too long pattern anchored at the end.
.endif
.if ${WORDS:S,^of$,,} != "sequences letters"
.  warning The :S modifier does not match a word anchored at both ends.
.endif
.if ${WORDS:S,^o$,,} != ${WORDS}
.  warning The :S modifier matches a prefix anchored at both ends.
.endif
.if ${WORDS:S,^f$,,} != ${WORDS}
.  warning The :S modifier matches a suffix anchored at both ends.
.endif
.if ${WORDS:S,^eof$,,} != ${WORDS}
.  warning The :S modifier matches a too long prefix anchored at both ends.
.endif
.if ${WORDS:S,^office$,,} != ${WORDS}
.  warning The :S modifier matches a too long suffix anchored at both ends.
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
	@echo ${:U1 2 3:S"2"two":Q} double quotes
	# In shell command lines, the hash does not need to be escaped.
	# It needs to be escaped in variable assignment lines though.
	@echo ${:U1 2 3:S#2#two#:Q} hash
	@echo ${:U1 2 3:S$2$two$:Q} dollar
	@echo ${:U1 2 3:S%2%two%:Q} percent
	@echo ${:U1 2 3:S'2'two':Q} apostrophe
	@echo ${:U1 2 3:S(2(two(:Q} opening parenthesis
	@echo ${:U1 2 3:S)2)two):Q} closing parenthesis
	@echo ${:U1 2 3:S121two1:Q} digit
	@echo ${:U1 2 3:S:2:two::Q} colon
	@echo ${:U1 2 3:S<2<two<:Q} less than sign
	@echo ${:U1 2 3:S=2=two=:Q} equal sign
	@echo ${:U1 2 3:S>2>two>:Q} greater than sign
	@echo ${:U1 2 3:S?2?two?:Q} question mark
	@echo ${:U1 2 3:S@2@two@:Q} at
	@echo ${:U1 2 3:Sa2atwoa:Q} letter
	@echo ${:U1 2 3:S[2[two[:Q} opening bracket
	@echo ${:U1 2 3:S\2\two\:Q} backslash
	@echo ${:U1 2 3:S]2]two]:Q} closing bracket
	@echo ${:U1 2 3:S^2^two^:Q} caret
	@echo ${:U1 2 3:S{2{two{:Q} opening brace
	@echo ${:U1 2 3:S|2|two|:Q} vertical line
	@echo ${:U1 2 3:S}2}two}:Q} closing brace
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
	@echo ${:Uvalue:S,a,x,i}.

# No matter how many dollar characters there are, they all get merged
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
# Here is an alternative way to generate dollar characters.
# It's unexpectedly complicated though.
	@echo $@:${:U:range=5:ts\x24:C,[0-9],,g:Q}:
# In modifiers, dollars are escaped using the backslash, not using another
# dollar sign.  Therefore, creating a dollar sign is pretty simple:
	@echo $@:${:Ugood3:S,^,\$\$\$,:Q}
