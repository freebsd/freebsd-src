# $NetBSD: moderrs.mk,v 1.31 2023/11/19 22:32:44 rillig Exp $
#
# various modifier error tests

'=		'\''
VAR=		TheVariable
# in case we have to change it ;-)
MOD_UNKN=	Z
MOD_TERM=	S,V,v
MOD_S:=		${MOD_TERM},

FIB=	1 1 2 3 5 8 13 21 34

all:	mod-unknown-direct mod-unknown-indirect
all:	unclosed-direct unclosed-indirect
all:	unfinished-indirect unfinished-loop
all:	loop-close
all:	words
all:	exclam
all:	mod-subst-delimiter
all:	mod-regex-delimiter
all:	mod-ts-parse
all:	mod-t-parse
all:	mod-ifelse-parse
all:	mod-remember-parse
all:	mod-sysv-parse

mod-unknown-direct: print-header print-footer
	@echo 'want: Unknown modifier $'Z$''
	@echo 'VAR:Z=before-${VAR:Z}-after'

mod-unknown-indirect: print-header print-footer
	@echo 'want: Unknown modifier $'Z$''
	@echo 'VAR:${MOD_UNKN}=before-${VAR:${MOD_UNKN}:inner}-after'

unclosed-direct: print-header print-footer
	@echo 'want: Unclosed expression, expecting $'}$' for modifier "S,V,v," of variable "VAR" with value "Thevariable"'
	@echo VAR:S,V,v,=${VAR:S,V,v,

unclosed-indirect: print-header print-footer
	@echo 'want: Unclosed expression after indirect modifier, expecting $'}$' for variable "VAR"'
	@echo VAR:${MOD_TERM},=${VAR:${MOD_S}

unfinished-indirect: print-header print-footer
	@echo 'want: Unfinished modifier for VAR ($',$' missing)'
	-@echo "VAR:${MOD_TERM}=${VAR:${MOD_TERM}}"

unfinished-loop: print-header print-footer
	@echo 'want: Unfinished modifier for UNDEF ($'@$' missing)'
	@echo ${UNDEF:U1 2 3:@var}
	@echo 'want: Unfinished modifier for UNDEF ($'@$' missing)'
	@echo ${UNDEF:U1 2 3:@var@...}
	@echo ${UNDEF:U1 2 3:@var@${var}@}

# The closing brace after the ${var} is part of the replacement string.
# In ParseModifierPart, braces and parentheses don't have to be balanced.
# This is contrary to the :M, :N modifiers, where both parentheses and
# braces must be balanced.
# This is also contrary to the SysV modifier, where only the actually
# used delimiter (either braces or parentheses) must be balanced.
loop-close: print-header print-footer
	@echo ${UNDEF:U1 2 3:@var@${var}}...@
	@echo ${UNDEF:U1 2 3:@var@${var}}...@}

words: print-header print-footer
	@echo 'want: Unfinished modifier for UNDEF ($']$' missing)'
	@echo ${UNDEF:U1 2 3:[}
	@echo 'want: Unfinished modifier for UNDEF ($']$' missing)'
	@echo ${UNDEF:U1 2 3:[#}

	# out of bounds => empty
	@echo 13=${UNDEF:U1 2 3:[13]}

	# Word index out of bounds.
	#
	# Until 2020-11-01, the behavior in this case depended upon the size
	# of unsigned long.
	#
	# On LP64I32, strtol returns LONG_MAX, which was then truncated to
	# int (undefined behavior), typically resulting in -1.  This -1 was
	# interpreted as "the last word".
	#
	# On ILP32, strtol returns LONG_MAX, which is a large number.  This
	# resulted in a range from LONG_MAX - 1 to 3, which was empty.
	#
	# Since 2020-11-01, the numeric overflow is detected and generates an
	# error.  In the remainder of the text, the '$,' is no longer parsed
	# as part of a variable modifier, where it would have been interpreted
	# as an anchor to the :S modifier, but as a normal variable named ','.
	# That variable is undefined, resulting in an empty string.
	@echo 12345=${UNDEF:U1 2 3:[123451234512345123451234512345]:S,^$,ok,:S,^3$,ok,}

exclam: print-header print-footer
	@echo 'want: Unfinished modifier for VARNAME ($'!$' missing)'
	@echo ${VARNAME:!echo}
	# When the final exclamation mark is missing, there is no
	# fallback to the SysV substitution modifier.
	# If there were a fallback, the output would be "exclam",
	# and the above would have produced an "Unknown modifier '!'".
	@echo 'want: Unfinished modifier for ! ($'!$' missing)'
	@echo ${!:L:!=exclam}

mod-subst-delimiter: print-header print-footer
	@echo 1: ${VAR:S
	@echo 2: ${VAR:S,
	@echo 3: ${VAR:S,from
	@echo 4: ${VAR:S,from,
	@echo 5: ${VAR:S,from,to
	@echo 6: ${VAR:S,from,to,
	@echo 7: ${VAR:S,from,to,}

mod-regex-delimiter: print-header print-footer
	@echo 1: ${VAR:C
	@echo 2: ${VAR:C,
	@echo 3: ${VAR:C,from
	@echo 4: ${VAR:C,from,
	@echo 5: ${VAR:C,from,to
	@echo 6: ${VAR:C,from,to,
	@echo 7: ${VAR:C,from,to,}

mod-ts-parse: print-header print-footer
	@echo ${FIB:ts}
	@echo ${FIB:ts\65}	# octal 065 == U+0035 == '5'
	@echo ${FIB:ts\65oct}	# bad modifier
	@echo ${:U${FIB}:ts\65oct} # bad modifier, variable name is ""
	@echo ${FIB:tsxy}	# modifier too long

mod-t-parse: print-header print-footer
	@echo ${FIB:t
	@echo ${FIB:txy}
	@echo ${FIB:t}
	@echo ${FIB:t:M*}

mod-ifelse-parse: print-header print-footer
	@echo ${FIB:?
	@echo ${FIB:?then
	@echo ${FIB:?then:
	@echo ${FIB:?then:else
	@echo ${FIB:?then:else}

mod-remember-parse: print-header print-footer
	@echo ${FIB:_}		# ok
	@echo ${FIB:__}		# modifier name too long

mod-sysv-parse: print-header print-footer
	@echo ${FIB:3
	@echo ${FIB:3=
	@echo ${FIB:3=x3
	@echo ${FIB:3=x3}	# ok

print-header: .USEBEFORE
	@echo $@:
print-footer: .USE
	@echo
