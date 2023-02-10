# $NetBSD: varmod-remember.mk,v 1.9 2023/02/09 22:21:57 rillig Exp $
#
# Tests for the :_ modifier, which saves the current expression value
# in the _ variable or another, to be used later again.


# The ':_' modifier is typically used in situations where the value of an
# expression is needed at the same time as a sequence of numbers.  In these
# cases, the value of the expression is saved in the temporary variable '_',
# from where it is taken later in the same expression.
ABC=	${A B C:L:_:range:@i@$i=${_:[$i]}@}
DEF=	${D E F:L:_:range:@i@$i=${_:[$i]}@}
GHI=	${G H I:L:_:range:@i@$i=${_:[$i]}@}

ABC.global:=	${ABC}		# is evaluated in the global scope
.if ${ABC.global} != "1=A 2=B 3=C"
.  error
.endif

.if ${DEF} != "1=D 2=E 3=F"	# is evaluated in the command line scope
.  error
.endif

# Before var.c 1.1040 from 2023-02-09, the temporary variable '_' was placed
# in the scope of the current evaluation, which meant that after the first
# ':_' modifier had been evaluated in command line scope, all further
# evaluations in global scope could not overwrite the variable '_' anymore,
# as the command line scope takes precedence over the global scope.
# The expression ${GHI} therefore evaluated to '1=D 2=E 3=F', reusing the
# value of '_' from the previous evaluation in command line scope.
GHI.global:=	${GHI}		# is evaluated in the global scope
.if ${GHI.global} != "1=G 2=H 3=I"
.  error
.endif


# In the parameterized form, having the variable name on the right side of
# the = assignment operator looks confusing.  In almost all other situations,
# the variable name is on the left-hand side of the = operator, therefore
# '_=SAVED' looks like it would copy 'SAVED' to '_'.  Luckily, this modifier
# is only rarely needed.
.if ${1 2 3:L:@var@${var:_=SAVED:}@} != "1 2 3"
.  error
.elif ${SAVED} != "3"
.  error
.endif


# The ':_' modifier takes a variable name as optional argument.  Before var.c
# 1.867 from 2021-03-14, this variable name could refer to other variables,
# such as in 'VAR.$p'.  It was not possible to refer to 'VAR.${param}' though,
# as that form caused a parse error.  The cause for the parse error in
# '${...:_=VAR.${param}}' is that the variable name is parsed in an ad-hoc
# manner, stopping at the first ':', ')' or '}', without taking any nested
# expressions into account.  Due to this inconsistency that short expressions
# are possible but long expressions aren't, the name of the temporary variable
# is no longer expanded.
#
# TODO: Warn about the unusual variable name '$S'.
S=	INDIRECT_VARNAME
.if ${value:L:@var@${var:_=$S}@} != "value"
.  error
.elif defined(INDIRECT_VARNAME)
.  error
.endif


# When a variable using ':_' refers to another variable that also uses ':_',
# the value of the temporary variable '_' from the inner expression leaks into
# the evaluation of the outer expression.  If the expressions were evaluated
# independently, the last word of the result would be outer_='outer' instead.
INNER=	${inner:L:_:@i@$i inner_='$_'@}
OUTER=	${outer:L:_:@o@$o ${INNER} outer_='$_'@}
.if ${OUTER} != "outer inner inner_='inner' outer_='inner'"
.endif


all:
