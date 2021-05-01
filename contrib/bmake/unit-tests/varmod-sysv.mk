# $NetBSD: varmod-sysv.mk,v 1.12 2020/12/05 13:01:33 rillig Exp $
#
# Tests for the ${VAR:from=to} variable modifier, which replaces the suffix
# "from" with "to".  It can also use '%' as a wildcard.
#
# This modifier is applied when the other modifiers don't match exactly.
#
# See ApplyModifier_SysV.

# A typical use case for the :from=to modifier is conversion of filename
# extensions.
.if ${src.c:L:.c=.o} != "src.o"
.  error
.endif

# The modifier applies to each word on its own.
.if ${one.c two.c three.c:L:.c=.o} != "one.o two.o three.o"
.  error
.endif

# Words that don't match the pattern are passed unmodified.
.if ${src.c src.h:L:.c=.o} != "src.o src.h"
.  error
.endif

# The :from=to modifier is therefore often combined with the :M modifier.
.if ${src.c src.h:L:M*.c:.c=.o} != "src.o"
.  error
.endif

# Another use case for the :from=to modifier is to append a suffix to each
# word.  In this case, the "from" string is empty, therefore it always
# matches.  The same effect can be achieved with the :S,$,teen, modifier.
.if ${four six seven nine:L:=teen} != "fourteen sixteen seventeen nineteen"
.  error
.endif

# The :from=to modifier can also be used to surround each word by strings.
# It might be tempting to use this for enclosing a string in quotes for the
# shell, but that's the job of the :Q modifier.
.if ${one two three:L:%=(%)} != "(one) (two) (three)"
.  error
.endif

# When the :from=to modifier is parsed, it lasts until the closing brace
# or parenthesis.  The :Q in the below expression may look like a modifier
# but isn't.  It is part of the replacement string.
.if ${a b c d e:L:%a=x:Q} != "x:Q b c d e"
.  error
.endif

# In the :from=to modifier, both parts can contain variable expressions.
.if ${one two:L:${:Uone}=${:U1}} != "1 two"
.  error
.endif

# In the :from=to modifier, the "from" part is expanded exactly once.
.if ${:U\$ \$\$ \$\$\$\$:${:U\$\$\$\$}=4} != "\$ \$\$ 4"
.  error
.endif

# In the :from=to modifier, the "to" part is expanded exactly twice.
# XXX: The right-hand side should be expanded only once.
# XXX: It's hard to get the escaping correct here, and to read that.
# XXX: It's not intuitive why the closing brace must be escaped but not
#      the opening brace.
.if ${:U1 2 4:4=${:Uonce\${\:Utwice\}}} != "1 2 oncetwice"
.  error
.endif

# The replacement string can contain spaces, thereby changing the number
# of words in the variable expression.
.if ${In:L:%=% ${:Uthe Sun}} != "In the Sun"
.  error
.endif

# If the variable value is empty, it is debatable whether it consists of a
# single empty word, or no word at all.  The :from=to modifier treats it as
# no word at all.
#
# See SysVMatch, which doesn't handle w_len == p_len specially.
.if ${:L:=suffix} != ""
.  error
.endif

# If the variable value is empty, it is debatable whether it consists of a
# single empty word (before 2020-05-06), or no word at all (since 2020-05-06).
#
# See SysVMatch, percent != NULL && w[0] == '\0'.
.if ${:L:%=suffix} != ""
.  error
.endif

# Before 2020-07-19, an ampersand could be used in the replacement part
# of a SysV substitution modifier, and it was replaced with the whole match,
# just like in the :S modifier.
#
# This was probably a copy-and-paste mistake since the code for the SysV
# modifier looked a lot like the code for the :S and :C modifiers.
# The ampersand is not mentioned in the manual page.
.if ${a.bcd.e:L:a.%=%} != "bcd.e"
.  error
.endif
# Before 2020-07-19, the result of the expression was "a.bcd.e".
.if ${a.bcd.e:L:a.%=&} != "&"
.  error
.endif

# Before 2020-07-20, when a SysV modifier was parsed, a single dollar
# before the '=' was parsed (but not interpreted) as an anchor.
# Parsing something without then evaluating it accordingly doesn't make
# sense.
.if ${value:L:e$=x} != "value"
.  error
.endif
# Before 2020-07-20, the modifier ":e$=x" was parsed as having a left-hand
# side "e" and a right-hand side "x".  The dollar was parsed (but not
# interpreted) as 'anchor at the end'.  Therefore the modifier was equivalent
# to ":e=x", which doesn't match the string "value$".  Therefore the whole
# expression evaluated to "value$".
.if ${${:Uvalue\$}:L:e$=x} != "valux"
.  error
.endif
.if ${value:L:e=x} != "valux"
.  error
.endif

# Words that don't match are copied unmodified.
.if ${:Ufile.c file.h:%.c=%.cpp} != "file.cpp file.h"
.  error
.endif

# The % placeholder can be anywhere in the string, it doesn't have to be at
# the beginning of the pattern.
.if ${:Ufile.c other.c:file.%=renamed.%} != "renamed.c other.c"
.  error
.endif

# It's also possible to modify each word by replacing the prefix and adding
# a suffix.
.if ${one two:L:o%=a%w} != "anew two"
.  error
.endif

# Each word gets the suffix "X" appended.
.if ${one two:L:=X} != "oneX twoX"
.  error
.endif

# The suffix "o" is replaced with "X".
.if ${one two:L:o=X} != "one twX"
.  error
.endif

# The suffix "o" is replaced with nothing.
.if ${one two:L:o=} != "one tw"
.  error
.endif

# The suffix "o" is replaced with a literal percent.  The percent is only
# a wildcard when it appears on the left-hand side.
.if ${one two:L:o=%} != "one tw%"
.  error
.endif

# Each word with the suffix "o" is replaced with "X".  The percent is a
# wildcard even though the right-hand side does not contain another percent.
.if ${one two:L:%o=X} != "one X"
.  error
.endif

# Each word with the prefix "o" is replaced with "X".  The percent is a
# wildcard even though the right-hand side does not contain another percent.
.if ${one two:L:o%=X} != "X two"
.  error
.endif

# For each word with the prefix "o" and the suffix "e", the whole word is
# replaced with "X".
.if ${one two oe oxen:L:o%e=X} != "X two X oxen"
.  error
.endif

# Only the first '%' is the wildcard.
.if ${one two o%e other%e:L:o%%e=X} != "one two X X"
.  error
.endif

# In the replacement, only the first '%' is the placeholder, all others
# are literal percent characters.
.if ${one two:L:%=%%} != "one% two%"
.  error
.endif

# In the word "one", only a prefix of the pattern suffix "nes" matches,
# the whole word is too short.  Therefore it doesn't match.
.if ${one two:L:%nes=%xxx} != "one two"
.  error
.endif

# The :from=to modifier can be used to replace both the prefix and a suffix
# of a word with other strings.  This is not possible with a single :S
# modifier, and using a :C modifier for the same task looks more complicated
# in many cases.
.if ${prefix-middle-suffix:L:prefix-%-suffix=p-%-s} != "p-middle-s"
.  error
.endif

# This is not a SysV modifier since the nested variable expression expands
# to an empty string.  The '=' in it should be irrelevant during parsing.
# XXX: As of 2020-12-05, this expression generates an "Unfinished modifier"
# error, while the correct error message would be "Unknown modifier" since
# there is no modifier named "fromto".
.if ${word214:L:from${:D=}to}
.  error
.endif

# XXX: This specially constructed case demonstrates that the SysV modifier
# lasts longer than expected.  The whole expression initially has the value
# "fromto}...".  The next modifier is a SysV modifier.  ApplyModifier_SysV
# parses the modifier as "from${:D=}to", ending at the '}'.  Next, the two
# parts of the modifier are parsed using ParseModifierPart, which scans
# differently, properly handling nested variable expressions.  The two parts
# are now "fromto}..." and "replaced".
.if "${:Ufromto\}...:from${:D=}to}...=replaced}" != "replaced"
.  error
.endif

# As of 2020-10-06, the right-hand side of the SysV modifier is expanded
# twice.  The first expansion happens in ApplyModifier_SysV, where the
# modifier is split into its two parts.  The second expansion happens
# when each word is replaced in ModifyWord_SYSVSubst.
# XXX: This is unexpected.  Add more test case to demonstrate the effects
# of removing one of the expansions.
VALUE=		value
INDIRECT=	1:${VALUE} 2:$${VALUE} 4:$$$${VALUE}
.if ${x:L:x=${INDIRECT}} != "1:value 2:value 4:\${VALUE}"
.  error
.endif

all:
