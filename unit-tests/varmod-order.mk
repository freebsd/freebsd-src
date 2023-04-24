# $NetBSD: varmod-order.mk,v 1.10 2023/02/27 08:29:36 rillig Exp $
#
# Tests for the :O variable modifier and its variants, which either sort the
# words of the value or shuffle them.

WORDS=		one two three four five six seven eight nine ten
NUMBERS=	8 5 4 9 1 7 6 10 3 2	# in English alphabetical order

.if ${WORDS:O} != "eight five four nine one seven six ten three two"
.  error ${WORDS:O}
.endif

# Unknown modifier "OX"
_:=	${WORDS:OX}

# Unknown modifier "OxXX"
_:=	${WORDS:OxXX}

# Missing closing brace, to cover the error handling code.
_:=	${WORDS:O
_:=	${NUMBERS:On
_:=	${NUMBERS:Onr

# Shuffling numerically doesn't make sense, so don't allow 'x' and 'n' to be
# combined.
#
# expect: make: Bad modifier ":Oxn" for variable "NUMBERS"
# expect+1: Malformed conditional (${NUMBERS:Oxn})
.if ${NUMBERS:Oxn}
.  error
.else
.  error
.endif

# Extra characters after ':On' are detected and diagnosed.
# TODO: Add line number information to the "Bad modifier" diagnostic.
#
# expect: make: Bad modifier ":On_typo" for variable "NUMBERS"
.if ${NUMBERS:On_typo}
.  error
.else
.  error
.endif

# Extra characters after ':Onr' are detected and diagnosed.
#
# expect: make: Bad modifier ":Onr_typo" for variable "NUMBERS"
.if ${NUMBERS:Onr_typo}
.  error
.else
.  error
.endif

# Extra characters after ':Orn' are detected and diagnosed.
#
# expect: make: Bad modifier ":Orn_typo" for variable "NUMBERS"
.if ${NUMBERS:Orn_typo}
.  error
.else
.  error
.endif

# Repeating the 'n' is not supported.  In the typical use cases, the sorting
# criteria are fixed, not computed, therefore allowing this redundancy does
# not make sense.
#
# expect: make: Bad modifier ":Onn" for variable "NUMBERS"
.if ${NUMBERS:Onn}
.  error
.else
.  error
.endif

# Repeating the 'r' is not supported as well, for the same reasons as above.
#
# expect: make: Bad modifier ":Onrr" for variable "NUMBERS"
.if ${NUMBERS:Onrr}
.  error
.else
.  error
.endif

# Repeating the 'r' is not supported as well, for the same reasons as above.
#
# expect: make: Bad modifier ":Orrn" for variable "NUMBERS"
.if ${NUMBERS:Orrn}
.  error
.else
.  error
.endif


# If a modifier that starts with ':O' is not one of the known sort or shuffle
# forms, it is a parse error.  Several other modifiers such as ':H' or ':u'
# fall back to the SysV modifier, for example, ':H=new' is not the standard
# ':H' modifier but instead replaces a trailing 'H' with 'new' in each word.
# There is no such fallback for the ':O' modifiers.
SWITCH=	On
# expect: make: Bad modifier ":On=Off" for variable "SWITCH"
.if ${SWITCH:On=Off} != "Off"
.  error
.else
.  error
.endif

all:
