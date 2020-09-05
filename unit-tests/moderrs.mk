# $Id: moderrs.mk,v 1.1.1.8 2020/08/26 16:40:43 sjg Exp $
#
# various modifier error tests

VAR=TheVariable
# incase we have to change it ;-)
MOD_UNKN=Z
MOD_TERM=S,V,v
MOD_S:= ${MOD_TERM},

FIB=	1 1 2 3 5 8 13 21 34

all:	modunkn modunknV varterm vartermV modtermV modloop
all:	modloop-close
all:	modwords
all:	modexclam
all:	mod-subst-delimiter
all:	mod-regex-delimiter
all:	mod-regex-undefined-subexpression
all:	mod-ts-parse
all:	mod-t-parse
all:	mod-ifelse-parse
all:	mod-remember-parse
all:	mod-sysv-parse

modunkn:
	@echo "Expect: Unknown modifier 'Z'"
	@echo "VAR:Z=${VAR:Z}"

modunknV:
	@echo "Expect: Unknown modifier 'Z'"
	@echo "VAR:${MOD_UNKN}=${VAR:${MOD_UNKN}}"

varterm:
	@echo "Expect: Unclosed variable specification for VAR"
	@echo VAR:S,V,v,=${VAR:S,V,v,

vartermV:
	@echo "Expect: Unclosed variable specification for VAR"
	@echo VAR:${MOD_TERM},=${VAR:${MOD_S}

modtermV:
	@echo "Expect: Unfinished modifier for VAR (',' missing)"
	-@echo "VAR:${MOD_TERM}=${VAR:${MOD_TERM}}"

modloop:
	@echo "Expect: 2 errors about missing @ delimiter"
	@echo ${UNDEF:U1 2 3:@var}
	@echo ${UNDEF:U1 2 3:@var@...}
	@echo ${UNDEF:U1 2 3:@var@${var}@}

# The closing brace after the ${var} is part of the replacement string.
# In ParseModifierPart, braces and parentheses don't have to be balanced.
# This is contrary to the :M, :N modifiers, where both parentheses and
# braces must be balanced.
# This is also contrary to the SysV modifier, where only the actually
# used delimiter (either braces or parentheses) must be balanced.
modloop-close:
	@echo $@:
	@echo ${UNDEF:U1 2 3:@var@${var}}...@
	@echo ${UNDEF:U1 2 3:@var@${var}}...@}

modwords:
	@echo "Expect: 2 errors about missing ] delimiter"
	@echo ${UNDEF:U1 2 3:[}
	@echo ${UNDEF:U1 2 3:[#}

	# out of bounds => empty
	@echo 13=${UNDEF:U1 2 3:[13]}

	# Word index out of bounds.
	#
	# On LP64I32, strtol returns LONG_MAX,
	# which is then truncated to int (undefined behavior),
	# typically resulting in -1.
	# This -1 is interpreted as "the last word".
	#
	# On ILP32, strtol returns LONG_MAX,
	# which is a large number.
	# This results in a range from LONG_MAX - 1 to 3,
	# which is empty.
	@echo 12345=${UNDEF:U1 2 3:[123451234512345123451234512345]:S,^$,ok,:S,^3$,ok,}

modexclam:
	@echo "Expect: 2 errors about missing ! delimiter"
	@echo ${VARNAME:!echo}
	# When the final exclamation mark is missing, there is no
	# fallback to the SysV substitution modifier.
	# If there were a fallback, the output would be "exclam",
	# and the above would have produced an "Unknown modifier '!'".
	@echo ${!:L:!=exclam}

mod-subst-delimiter:
	@echo $@:
	@echo ${VAR:S
	@echo ${VAR:S,
	@echo ${VAR:S,from
	@echo ${VAR:S,from,
	@echo ${VAR:S,from,to
	@echo ${VAR:S,from,to,
	@echo ${VAR:S,from,to,}
	@echo 1: ${VAR:S
	@echo 2: ${VAR:S,
	@echo 3: ${VAR:S,from
	@echo ${VAR:S,from,
	@echo ${VAR:S,from,to
	@echo ${VAR:S,from,to,
	@echo ${VAR:S,from,to,}

mod-regex-delimiter:
	@echo $@:
	@echo ${VAR:C
	@echo ${VAR:C,
	@echo ${VAR:C,from
	@echo ${VAR:C,from,
	@echo ${VAR:C,from,to
	@echo ${VAR:C,from,to,
	@echo ${VAR:C,from,to,}
	@echo 1: ${VAR:C
	@echo 2: ${VAR:C,
	@echo 3: ${VAR:C,from
	@echo ${VAR:C,from,
	@echo ${VAR:C,from,to
	@echo ${VAR:C,from,to,
	@echo ${VAR:C,from,to,}

# In regular expressions with alternatives, not all capturing groups are
# always set; some may be missing.  Warn about these.
#
# Since there is no way to turn off this warning, the combination of
# alternative matches and capturing groups is not widely used.
#
# A newly added modifier 'U' such as in :C,(a.)|(b.),\1\2,U might be added
# for treating undefined capturing groups as empty, but that would create a
# syntactical ambiguity since the :S and :C modifiers are open-ended (see
# mod-subst-chain).  Luckily the modifier :U does not make sense after :C,
# therefore this case does not happen in practice.
# The sub-modifier for the :C modifier would have to be chosen wisely.
mod-regex-undefined-subexpression:
	@echo $@:
	@echo ${FIB:C,1(.*),one\1,}		# all ok
	@echo ${FIB:C,1(.*)|2(.*),(\1)+(\2),:Q}	# no match for subexpression

mod-ts-parse:
	@echo $@:
	@echo ${FIB:ts}
	@echo ${FIB:ts\65}	# octal 065 == U+0035 == '5'
	@echo ${FIB:ts\65oct}	# bad modifier
	@echo ${FIB:tsxy}	# modifier too long

mod-t-parse:
	@echo $@:
	@echo ${FIB:t
	@echo ${FIB:txy}
	@echo ${FIB:t}
	@echo ${FIB:t:M*}

mod-ifelse-parse:
	@echo $@:
	@echo ${FIB:?
	@echo ${FIB:?then
	@echo ${FIB:?then:
	@echo ${FIB:?then:else
	@echo ${FIB:?then:else}

mod-remember-parse:
	@echo $@:
	@echo ${FIB:_}		# ok
	@echo ${FIB:__}		# modifier name too long

mod-sysv-parse:
	@echo $@:
	@echo ${FIB:3
	@echo ${FIB:3=
	@echo ${FIB:3=x3
	@echo ${FIB:3=x3}	# ok
