# $NetBSD: directive-for-escape.mk,v 1.3 2020/12/31 14:26:37 rillig Exp $
#
# Test escaping of special characters in the iteration values of a .for loop.
# These values get expanded later using the :U variable modifier, and this
# escaping and unescaping must pass all characters and strings effectively
# unmodified.

.MAKEFLAGS: -df

# Even though the .for loops takes quotes into account when splitting the
# string into words, the quotes don't need to be balances, as of 2020-12-31.
# This could be considered a bug.
ASCII=	!"\#$$%&'()*+,-./0-9:;<=>?@A-Z[\]_^a-z{|}~

# XXX: As of 2020-12-31, the '#' is not preserved in the expanded body of
# the loop since it would not need only the escaping for the :U variable
# modifier but also the escaping for the line-end comment.
.for chars in ${ASCII}
.  info ${chars}
.endfor

# As of 2020-12-31, using 2 backslashes before be '#' would treat the '#'
# as comment character.  Using 3 backslashes doesn't help either since
# then the situation is essentially the same as with 1 backslash.
# This means that a '#' sign cannot be passed in the value of a .for loop
# at all.
ASCII.2020-12-31=	!"\\\#$$%&'()*+,-./0-9:;<=>?@A-Z[\]_^a-z{|}~
.for chars in ${ASCII.2020-12-31}
.  info ${chars}
.endfor

# Cover the code in for_var_len.
#
# XXX: It is unexpected that the variable V gets expanded in the loop body.
# The double '$$' should prevent exactly this.  Probably nobody was
# adventurous enough to use literal dollar signs in the values for a .for
# loop.
V=		value
VALUES=		$$ $${V} $${V:=-with-modifier} $$(V) $$(V:=-with-modifier)
.for i in ${VALUES}
.  info $i
.endfor

# Cover the code for nested '{}' in for_var_len.
#
# The value of VALUES is not a variable expression.  Instead, it is meant to
# represent dollar, lbrace, "UNDEF:U", backslash, dollar, backslash, dollar,
# space, nested braces, space, "end}".
VALUES=		$${UNDEF:U\$$\$$ {{}} end}
# XXX: Where does the '\$$\$$' get converted into a single '\$'?
.for i in ${VALUES}
.  info $i
.endfor

# A single trailing dollar doesn't happen in practice.
# The dollar sign is correctly passed through to the body of the .for loop.
# There, it is expanded by the .info directive, but even there a trailing
# dollar sign is kept as-is.
.for i in ${:U\$}
.  info ${i}
.endfor

# As of 2020-12-31, the name of the iteration variable can even contain
# colons, which then affects variable expressions having this exact modifier.
# This is clearly an unintended side effect of the implementation.
NUMBERS=	one two three
.for NUMBERS:M*e in replaced
.  info ${NUMBERS} ${NUMBERS:M*e}
.endfor

# As of 2020-12-31, the name of the iteration variable can contain braces,
# which gets even more surprising than colons, since it allows to replace
# sequences of variable expressions.  There is no practical use case for
# this, though.
BASENAME=	one
EXT=		.c
.for BASENAME}${EXT in replaced
.  info ${BASENAME}${EXT}
.endfor

# Demonstrate the various ways to refer to the iteration variable.
i=		outer
i2=		two
i,=		comma
.for i in inner
.  info .        $$i: $i
.  info .      $${i}: ${i}
.  info .   $${i:M*}: ${i:M*}
.  info .      $$(i): $(i)
.  info .   $$(i:M*): $(i:M*)
.  info . $${i$${:U}}: ${i${:U}}
.  info .    $${i\}}: ${i\}}	# XXX: unclear why SubstVarLong needs this
.  info .     $${i2}: ${i2}
.  info .     $${i,}: ${i,}
.  info .  adjacent: $i${i}${i:M*}$i
.endfor
