# $NetBSD: var-op-expand.mk,v 1.11 2021/01/01 23:07:48 sjg Exp $
#
# Tests for the := variable assignment operator, which expands its
# right-hand side.

.MAKE.SAVE_DOLLARS:=      yes

# If the right-hand side does not contain a dollar sign, the ':=' assignment
# operator has the same effect as the '=' assignment operator.
VAR:=			value
.if ${VAR} != "value"
.  error
.endif

# When a ':=' assignment is performed, its right-hand side is evaluated and
# expanded as far as possible.  Contrary to other situations, '$$' and
# variable expressions based on undefined variables are preserved though.
#
# Whether a variable expression is undefined or not is determined at the end
# of evaluating the expression.  The consequence is that ${:Ufallback} expands
# to "fallback"; initially this expression is undefined since it is based on
# the variable named "", which is guaranteed to be never defined, but at the
# end of evaluating the expression ${:Ufallback}, the modifier ':U' has turned
# the expression into a defined expression.


# literal dollar signs
VAR:=		$$ $$$$ $$$$$$$$
.if ${VAR} != "\$ \$\$ \$\$\$\$"
.  error
.endif


# reference to a variable containing a literal dollar sign
REF=		$$ $$$$ $$$$$$$$
VAR:=		${REF}
REF=		too late
.if ${VAR} != "\$ \$\$ \$\$\$\$"
.  error
.endif


# reference to an undefined variable
.undef UNDEF
VAR:=		<${UNDEF}>
UNDEF=		after
.if ${VAR} != "<after>"
.  error
.endif


# reference to a variable whose name is computed from another variable
REF2=		referred to
REF=		REF2
VAR:=		${${REF}}
REF=		too late
.if ${VAR} != "referred to"
.  error
.endif


# expression with an indirect modifier referring to an undefined variable
.undef UNDEF
VAR:=		${:${UNDEF}}
UNDEF=		Uwas undefined
.if ${VAR} != "was undefined"
.  error
.endif


# expression with an indirect modifier referring to another variable that
# in turn refers to an undefined variable
#
# XXX: Even though this is a ':=' assignment, the '${UNDEF}' in the part of
# the variable modifier is not preserved.  To preserve it, ParseModifierPart
# would have to call VarSubstExpr somehow since this is the only piece of
# code that takes care of this global variable.
.undef UNDEF
REF=		U${UNDEF}
#.MAKEFLAGS: -dv
VAR:=		${:${REF}}
#.MAKEFLAGS: -d0
REF=		too late
UNDEF=		Uwas undefined
.if ${VAR} != ""
.  error
.endif


# In variable assignments using the ':=' operator, undefined variables are
# preserved, no matter how indirectly they are referenced.
.undef REF3
REF2=		<${REF3}>
REF=		${REF2}
VAR:=		${REF}
REF3=		too late
.if ${VAR} != "<too late>"
.  error
.endif


# In variable assignments using the ':=' operator, '$$' are preserved, no
# matter how indirectly they are referenced.
REF2=		REF2:$$ $$$$
REF=		REF:$$ $$$$ ${REF2}
VAR:=		VAR:$$ $$$$ ${REF}
.if ${VAR} != "VAR:\$ \$\$ REF:\$ \$\$ REF2:\$ \$\$"
.  error
.endif


# In variable assignments using the ':=' operator, '$$' are preserved in the
# expressions of the top level, but not in expressions that are nested.
VAR:=		top:$$ ${:Unest1\:\$\$} ${:Unest2${:U\:\$\$}}
.if ${VAR} != "top:\$ nest1:\$ nest2:\$"
.  error
.endif


# In variable assignments using the ':=' operator, there may be expressions
# containing variable modifiers, and these modifiers may refer to other
# variables.  These referred-to variables are expanded at the time of
# assignment.  The undefined variables are kept as-is and are later expanded
# when evaluating the condition.
#
# Contrary to the assignment operator '=', the assignment operator ':='
# consumes the '$' from modifier parts.
REF.word=	1:$$ 2:$$$$ 4:$$$$$$$$
.undef REF.undef
VAR:=		${:Uword undef:@word@${REF.${word}}@}, direct: ${REF.word} ${REF.undef}
REF.word=	word.after
REF.undef=	undef.after
.if ${VAR} != "1:2:\$ 4:\$\$ undef.after, direct: 1:\$ 2:\$\$ 4:\$\$\$\$ undef.after"
.  error
.endif

# Just for comparison, the previous example using the assignment operator '='
# instead of ':='.  The right-hand side of the assignment is not evaluated at
# the time of assignment but only later, when ${VAR} appears in the condition.
#
# At that point, both REF.word and REF.undef are defined.
REF.word=	1:$$ 2:$$$$ 4:$$$$$$$$
.undef REF.undef
VAR=		${:Uword undef:@word@${REF.${word}}@}, direct: ${REF.word} ${REF.undef}
REF.word=	word.after
REF.undef=	undef.after
.if ${VAR} != "word.after undef.after, direct: word.after undef.after"
.  error
.endif


# Between var.c 1.42 from 2000-05-11 and before parse.c 1.520 from 2020-12-27,
# if the variable name in a ':=' assignment referred to an undefined variable,
# there were actually 2 assignments to different variables:
#
#	Global["VAR_SUBST_${UNDEF}"] = ""
#	Global["VAR_SUBST_"] = ""
#
# The variable name with the empty value actually included a dollar sign.
# Variable names with dollars are not used in practice.
#
# It might be a good idea to forbid undefined variables on the left-hand side
# of a variable assignment.
.undef UNDEF
VAR_ASSIGN_${UNDEF}=	assigned by '='
VAR_SUBST_${UNDEF}:=	assigned by ':='
.if ${VAR_ASSIGN_} != "assigned by '='"
.  error
.endif
.if defined(${:UVAR_SUBST_\${UNDEF\}})
.  error
.endif
.if ${VAR_SUBST_} != "assigned by ':='"
.  error
.endif

all:
	@:;
