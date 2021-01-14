# $NetBSD: directive-if.mk,v 1.9 2020/12/19 22:33:11 rillig Exp $
#
# Tests for the .if directive.
#
# See also:
#	cond-*.mk

# TODO: Implementation

.if 0
.  error
.else
.  info 0 evaluates to false.
.endif

.if 1
.  info 1 evaluates to true.
.else
.  error
.endif

# There is no '.ifx'.
#
# The commit from 2005-05-01 intended to detect this situation, but it failed
# to do this since the call to is_token had its arguments switched.  They were
# expected as (str, token, token_len) but were actually passed as (token, str,
# token_len).  This made is_token return true even if the directive was
# directly followed by alphanumerical characters, which was wrong.  The
# typical cases produced an error message such as "Malformed conditional
# (x 123)", while the intended error message was "Unknown directive".
#
# Back at that time, the commits only modified the main code but did not add
# the corresponding unit tests.  This allowed the bug to hide for more than
# 15 years.
#
# Since 2020-11-10, the correct error message is produced.  The '.ifx' is no
# longer interpreted as a variant of '.if', therefore the '.error' and '.else'
# are interpreted as ordinary directives, producing the error messages
# "if-less else" and "if-less endif".
.ifx 123
.info This is not conditional.
.else
.info This is not conditional.
.endif

# Missing condition.
.if
.  error
.else
.  error
.endif

# A plain word must not start with a '"'.  It may contain a embedded quotes
# though, which are kept.  The quotes need not be balanced.  The next space
# ends the word, and the remaining " || 1" is parsed as "or true".
.if ${:Uplain"""""} == plain""""" || 1
.  info Quotes in plain words are probably a mistake.
# XXX: Accepting quotes in plain words is probably a mistake as well.
.else
.  error
.endif

.if0
.  error
.else
.  info Don't do this, always put a space after a directive.
.endif

.if${:U-3}
.  info Don't do this, always put a space after a directive.
.else
.  error
.endif

.if${:U-3}>-4
.  info Don't do this, always put a space around comparison operators.
.else
.  error
.endif

.if(1)
.  info Don't do this, always put a space after a directive.
.endif

.if!0
.  info Don't do this, always put a space after a directive.
.endif

all:
