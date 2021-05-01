# $NetBSD: var-op-sunsh.mk,v 1.6 2020/11/15 20:20:58 rillig Exp $
#
# Tests for the :sh= variable assignment operator, which runs its right-hand
# side through the shell.  It is a seldom-used alternative to the !=
# assignment operator, adopted from Sun make.

.MAKEFLAGS: -dL			# Enable sane error messages

# This is the idiomatic form of the Sun shell assignment operator.
# The assignment operator is directly preceded by the ':sh'.
VAR:sh=		echo colon-sh
.if ${VAR} != "colon-sh"
.  error
.endif

# It is also possible to have whitespace around the :sh assignment
# operator modifier.
VAR :sh =	echo colon-sh-spaced
.if ${VAR} != "colon-sh-spaced"
.  error
.endif

# Until 2020-10-04, the ':sh' could even be followed by other characters.
# This was neither documented by NetBSD make nor by Solaris make and was
# an implementation error.
#
# Since 2020-10-04, this is a normal variable assignment using the '='
# assignment operator.
VAR:shell=	echo colon-shell
.if ${${:UVAR\:shell}} != "echo colon-shell"
.  error
.endif

# Several colons can syntactically appear in a variable name.
# Until 2020-10-04, the last of them was interpreted as the ':sh'
# assignment operator.
#
# Since 2020-10-04, the colons are part of the variable name.
VAR:shoe:shore=	echo two-colons
.if ${${:UVAR\:shoe\:shore}} != "echo two-colons"
.  error
.endif

# Until 2020-10-04, the following expression was wrongly marked as
# a parse error.  This was because the parser for variable assignments
# just looked for the previous ":sh", without taking any contextual
# information into account.
#
# There are two different syntactical elements that look exactly the same:
# The variable modifier ':sh' and the assignment operator modifier ':sh'.
# Intuitively this variable name contains the variable modifier, but until
# 2020-10-04, the parser regarded it as an assignment operator modifier, in
# Parse_DoVar.
VAR.${:Uecho 123:sh}=	ok-123
.if ${VAR.123} != "ok-123"
.  error
.endif

# Same pattern here. Until 2020-10-04, the ':sh' inside the nested expression
# was taken for the :sh assignment operator modifier, even though it was
# escaped by a backslash.
VAR.${:U echo\:shell}=	ok-shell
.if ${VAR.${:U echo\:shell}} != "ok-shell"
.  error
.endif

# Until 2020-10-04, the word 'shift' was also affected since it starts with
# ':sh'.
VAR.key:shift=		Shift
.if ${${:UVAR.key\:shift}} != "Shift"
.  error
.endif

# Just for fun: The code in Parse_IsVar allows for multiple appearances of
# the ':sh' assignment operator modifier.  Let's see what happens ...
#
# Well, the end result is correct but the way until there is rather
# adventurous.  This only works because the parser replaces each an every
# whitespace character that is not nested with '\0' (see Parse_DoVar).
# The variable name therefore ends before the first ':sh', and the last
# ':sh' turns the assignment operator into the shell command evaluation.
# Parse_DoVar completely trusts Parse_IsVar to properly verify the syntax.
#
# The ':sh' is the only word that may occur between the variable name and
# the assignment operator at nesting level 0.  All other words would lead
# to a parse error since the left-hand side of an assignment must be
# exactly one word.
VAR :sh :sh :sh :sh=	echo multiple
.if ${VAR} != "multiple"
.  error
.endif

# The word ':sh' is not the only thing that can occur after a variable name.
# Since the parser just counts braces and parentheses instead of properly
# expanding nested expressions, the token ' :sh' can be used to add arbitrary
# text between the variable name and the assignment operator, it just has to
# be enclosed in braces or parentheses.
VAR :sh(Put a comment here)=	comment in parentheses
.if ${VAR} != "comment in parentheses"
.  error
.endif

# The unintended comment can include multiple levels of nested braces and
# parentheses, they don't even need to be balanced since they are only
# counted by Parse_IsVar and ignored by Parse_DoVar.
VAR :sh{Put}((((a}{comment}}}}{here}=	comment in braces
.if ${VAR} != "comment in braces"
.  error
.endif

# Syntactically, the ':sh' modifier can be combined with the '+=' assignment
# operator.  In such a case the ':sh' modifier is silently ignored.
#
# XXX: This combination should not be allowed at all.
VAR=		one
VAR :sh +=	echo two
.if ${VAR} != "one echo two"
.  error ${VAR}
.endif

# TODO: test VAR:sh!=command

all:
	@:;
