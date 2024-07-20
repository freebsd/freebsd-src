# $NetBSD: moderrs.mk,v 1.38 2024/07/05 19:47:22 rillig Exp $
#
# various modifier error tests

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

mod-unknown-direct: print-footer
# expect: make: in target "mod-unknown-direct": while evaluating variable "VAR" with value "TheVariable": Unknown modifier "Z"
	@echo 'VAR:Z=before-${VAR:Z}-after'

mod-unknown-indirect: print-footer
# expect: make: in target "mod-unknown-indirect": while evaluating variable "VAR" with value "TheVariable": Unknown modifier "Z"
	@echo 'VAR:${MOD_UNKN}=before-${VAR:${MOD_UNKN}:inner}-after'

unclosed-direct: print-header print-footer
# expect: make: in target "unclosed-direct": while evaluating variable "VAR" with value "Thevariable": Unclosed expression, expecting '}' for modifier "S,V,v,"
	@echo VAR:S,V,v,=${VAR:S,V,v,

unclosed-indirect: print-header print-footer
# expect: make: in target "unclosed-indirect": while evaluating variable "VAR" with value "Thevariable": Unclosed expression after indirect modifier, expecting '}'
	@echo VAR:${MOD_TERM},=${VAR:${MOD_S}

unfinished-indirect: print-footer
# expect: make: in target "unfinished-indirect": while evaluating variable "VAR" with value "TheVariable": Unfinished modifier (',' missing)
	-@echo "VAR:${MOD_TERM}=${VAR:${MOD_TERM}}"

unfinished-loop: print-footer
# expect: make: in target "unfinished-loop": while evaluating variable "UNDEF" with value "1 2 3": Unfinished modifier ('@' missing)
	@echo ${UNDEF:U1 2 3:@var}
# expect: make: in target "unfinished-loop": while evaluating variable "UNDEF" with value "1 2 3": Unfinished modifier ('@' missing)
	@echo ${UNDEF:U1 2 3:@var@...}
	@echo ${UNDEF:U1 2 3:@var@${var}@}

# The closing brace after the ${var} is part of the replacement string.
# In ParseModifierPart, braces and parentheses don't have to be balanced.
# This is contrary to the :M, :N modifiers, where both parentheses and
# braces must be balanced.
# This is also contrary to the SysV modifier, where only the actually
# used delimiter (either braces or parentheses) must be balanced.
loop-close: print-header print-footer
# expect: make: in target "loop-close": while evaluating variable "UNDEF" with value "1}... 2}... 3}...": Unclosed expression, expecting '}' for modifier "@var@${var}}...@"
	@echo ${UNDEF:U1 2 3:@var@${var}}...@
	@echo ${UNDEF:U1 2 3:@var@${var}}...@}

words: print-footer
# expect: make: in target "words": while evaluating variable "UNDEF" with value "1 2 3": Unfinished modifier (']' missing)
	@echo ${UNDEF:U1 2 3:[}
# expect: make: in target "words": while evaluating variable "UNDEF" with value "1 2 3": Unfinished modifier (']' missing)
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

exclam: print-footer
# expect: make: in target "exclam": while evaluating variable "VARNAME" with value "": Unfinished modifier ('!' missing)
	@echo ${VARNAME:!echo}
	# When the final exclamation mark is missing, there is no
	# fallback to the SysV substitution modifier.
	# If there were a fallback, the output would be "exclam",
	# and the above would have produced an "Unknown modifier '!'".
# expect: make: in target "exclam": while evaluating variable "!" with value "!": Unfinished modifier ('!' missing)
	@echo ${!:L:!=exclam}

mod-subst-delimiter: print-footer
# expect: make: in target "mod-subst-delimiter": while evaluating variable "VAR" with value "TheVariable": Missing delimiter for modifier ':S'
	@echo 1: ${VAR:S
# expect: make: in target "mod-subst-delimiter": while evaluating variable "VAR" with value "TheVariable": Unfinished modifier (',' missing)
	@echo 2: ${VAR:S,
# expect: make: in target "mod-subst-delimiter": while evaluating variable "VAR" with value "TheVariable": Unfinished modifier (',' missing)
	@echo 3: ${VAR:S,from
# expect: make: in target "mod-subst-delimiter": while evaluating variable "VAR" with value "TheVariable": Unfinished modifier (',' missing)
	@echo 4: ${VAR:S,from,
# expect: make: in target "mod-subst-delimiter": while evaluating variable "VAR" with value "TheVariable": Unfinished modifier (',' missing)
	@echo 5: ${VAR:S,from,to
# expect: make: in target "mod-subst-delimiter": while evaluating variable "VAR" with value "TheVariable": Unclosed expression, expecting '}' for modifier "S,from,to,"
	@echo 6: ${VAR:S,from,to,
	@echo 7: ${VAR:S,from,to,}

mod-regex-delimiter: print-footer
# expect: make: in target "mod-regex-delimiter": while evaluating variable "VAR" with value "TheVariable": Missing delimiter for modifier ':C'
	@echo 1: ${VAR:C
# expect: make: in target "mod-regex-delimiter": while evaluating variable "VAR" with value "TheVariable": Unfinished modifier (',' missing)
	@echo 2: ${VAR:C,
# expect: make: in target "mod-regex-delimiter": while evaluating variable "VAR" with value "TheVariable": Unfinished modifier (',' missing)
	@echo 3: ${VAR:C,from
# expect: make: in target "mod-regex-delimiter": while evaluating variable "VAR" with value "TheVariable": Unfinished modifier (',' missing)
	@echo 4: ${VAR:C,from,
# expect: make: in target "mod-regex-delimiter": while evaluating variable "VAR" with value "TheVariable": Unfinished modifier (',' missing)
	@echo 5: ${VAR:C,from,to
# expect: make: in target "mod-regex-delimiter": while evaluating variable "VAR" with value "TheVariable": Unclosed expression, expecting '}' for modifier "C,from,to,"
	@echo 6: ${VAR:C,from,to,
	@echo 7: ${VAR:C,from,to,}

mod-ts-parse: print-header print-footer
	@echo ${FIB:ts}
	@echo ${FIB:ts\65}	# octal 065 == U+0035 == '5'
# expect: make: in target "mod-ts-parse": while evaluating variable "FIB" with value "1 1 2 3 5 8 13 21 34": Bad modifier ":ts\65oct"
	@echo ${FIB:ts\65oct}	# bad modifier
# expect: make: in target "mod-ts-parse": while evaluating "${:U${FIB}:ts\65oct} # bad modifier, variable name is """ with value "1 1 2 3 5 8 13 21 34": Bad modifier ":ts\65oct"
	@echo ${:U${FIB}:ts\65oct} # bad modifier, variable name is ""
# expect: make: in target "mod-ts-parse": while evaluating variable "FIB" with value "1 1 2 3 5 8 13 21 34": Bad modifier ":tsxy"
	@echo ${FIB:tsxy}	# modifier too long

mod-t-parse: print-header print-footer
# expect: make: in target "mod-t-parse": while evaluating variable "FIB" with value "1 1 2 3 5 8 13 21 34": Bad modifier ":t"
	@echo ${FIB:t
# expect: make: in target "mod-t-parse": while evaluating variable "FIB" with value "1 1 2 3 5 8 13 21 34": Bad modifier ":txy"
	@echo ${FIB:txy}
# expect: make: in target "mod-t-parse": while evaluating variable "FIB" with value "1 1 2 3 5 8 13 21 34": Bad modifier ":t"
	@echo ${FIB:t}
# expect: make: in target "mod-t-parse": while evaluating variable "FIB" with value "1 1 2 3 5 8 13 21 34": Bad modifier ":t"
	@echo ${FIB:t:M*}

mod-ifelse-parse: print-footer
# expect: make: in target "mod-ifelse-parse": while evaluating then-branch of condition "FIB": Unfinished modifier (':' missing)
	@echo ${FIB:?
# expect: make: in target "mod-ifelse-parse": while evaluating then-branch of condition "FIB": Unfinished modifier (':' missing)
	@echo ${FIB:?then
# expect: make: in target "mod-ifelse-parse": while evaluating else-branch of condition "FIB": Unfinished modifier ('}' missing)
	@echo ${FIB:?then:
# expect: make: in target "mod-ifelse-parse": while evaluating else-branch of condition "FIB": Unfinished modifier ('}' missing)
	@echo ${FIB:?then:else
	@echo ${FIB:?then:else}

mod-remember-parse: print-footer
	@echo ${FIB:_}		# ok
# expect: make: in target "mod-remember-parse": while evaluating variable "FIB" with value "1 1 2 3 5 8 13 21 34": Unknown modifier "__"
	@echo ${FIB:__}		# modifier name too long

mod-sysv-parse: print-footer
# expect: make: in target "mod-sysv-parse": while evaluating variable "FIB" with value "1 1 2 3 5 8 13 21 34": Unknown modifier "3"
# expect: make: in target "mod-sysv-parse": while evaluating variable "FIB" with value "": Unclosed expression, expecting '}' for modifier "3"
	@echo ${FIB:3
# expect: make: in target "mod-sysv-parse": while evaluating variable "FIB" with value "1 1 2 3 5 8 13 21 34": Unknown modifier "3="
# expect: make: in target "mod-sysv-parse": while evaluating variable "FIB" with value "": Unclosed expression, expecting '}' for modifier "3="
	@echo ${FIB:3=
# expect: make: in target "mod-sysv-parse": while evaluating variable "FIB" with value "1 1 2 3 5 8 13 21 34": Unknown modifier "3=x3"
# expect: make: in target "mod-sysv-parse": while evaluating variable "FIB" with value "": Unclosed expression, expecting '}' for modifier "3=x3"
	@echo ${FIB:3=x3
	@echo ${FIB:3=x3}	# ok

print-header: .USEBEFORE
	@echo $@:
print-footer: .USE
	@echo
