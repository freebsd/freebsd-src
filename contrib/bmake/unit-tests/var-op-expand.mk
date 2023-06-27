# $NetBSD: var-op-expand.mk,v 1.18 2023/06/01 20:56:35 rillig Exp $
#
# Tests for the := variable assignment operator, which expands its
# right-hand side.
#
# See also:
#	varname-dot-make-save_dollars.mk

# Force the test results to be independent of the default value of this
# setting, which is 'yes' for NetBSD's usr.bin/make but 'no' for the bmake
# distribution and pkgsrc/devel/bmake.
.MAKE.SAVE_DOLLARS:=	yes

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


# reference to a variable containing literal dollar signs
REF=		$$ $$$$ $$$$$$$$
VAR:=		${REF}
REF=		too late
.if ${VAR} != "\$ \$\$ \$\$\$\$"
.  error
.endif


# reference to an undefined variable
.undef UNDEF
VAR:=		<${UNDEF}>
.if ${VAR} != "<>"
.  error
.endif
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
.if ${VAR} != ""
.  error
.endif
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
.if ${VAR} != "<>"
.  error
.endif
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


# The following test case demonstrates that the variable 'LATER' is preserved
# in the ':=' assignment since the variable 'LATER' is not yet defined.
# After the assignment to 'LATER', evaluating the variable 'INDIRECT'
# evaluates 'LATER' as well.
#
.undef LATER
INDIRECT:=	${LATER:S,value,replaced,}
.if ${INDIRECT} != ""
.  error
.endif
LATER=	late-value
.if ${INDIRECT} != "late-replaced"
.  error
.endif


# Same as the test case above, except for the additional modifier ':tl' when
# evaluating the variable 'INDIRECT'.  Nothing surprising here.
.undef LATER
.undef later
INDIRECT:=	${LATER:S,value,replaced,}
.if ${INDIRECT:tl} != ""
.  error
.endif
LATER=	uppercase-value
later=	lowercase-value
.if ${INDIRECT:tl} != "uppercase-replaced"
.  error
.endif


# Similar to the two test cases above, the situation gets a bit more involved
# here, due to the double indirection.  The variable 'indirect' is supposed to
# be the lowercase version of the variable 'INDIRECT'.
#
# The assignment operator ':=' for the variable 'INDIRECT' could be a '=' as
# well, it wouldn't make a difference in this case.  The crucial detail is the
# assignment operator ':=' for the variable 'indirect'.  During this
# assignment, the variable modifier ':S,value,replaced,' is converted to
# lowercase, which turns 'S' into 's', thus producing an unknown modifier.
# In this case, make issues a warning, but in cases where the modifier
# includes a '=', the modifier would be interpreted as a SysV-style
# substitution like '.c=.o', and make would not issue a warning, leading to
# silent unexpected behavior.
#
# As of 2021-11-20, the actual behavior is unexpected.  Fixing it is not
# trivial.  When the assignment to 'indirect' takes place, the expressions
# from the nested expression could be preserved, like this:
#
#	Start with:
#
#		indirect:=	${INDIRECT:tl}
#
#	Since INDIRECT is defined, expand it, remembering that the modifier
#	':tl' must still be applied to the final result.
#
#		indirect:=	${LATER:S,value,replaced,} \
#				OK \
#				${LATER:value=sysv}
#
#	The variable 'LATER' is not defined.  An idea may be to append the
#	remaining modifier ':tl' to each expression that is starting with an
#	undefined variable, resulting in:
#
#		indirect:=	${LATER:S,value,replaced,:tl} \
#				OK \
#				${LATER:value=sysv:tl}
#
#	This would work for the first expression.  The second expression ends
#	with the SysV modifier ':from=to', and when this modifier is parsed,
#	it consumes all characters until the end of the expression, which in
#	this case would replace the suffix 'value' with the literal 'sysv:tl',
#	ignoring that the ':tl' was intended to be an additional modifier.
#
# Due to all of this, this surprising behavior is not easy to fix.
#
.undef LATER
.undef later
INDIRECT:=	${LATER:S,value,replaced,} OK ${LATER:value=sysv}
indirect:=	${INDIRECT:tl}
# expect+1: Unknown modifier "s,value,replaced,"
.if ${indirect} != " ok "
.  error
.else
# expect+1: warning: XXX Neither branch should be taken.
.  warning	XXX Neither branch should be taken.
.endif
LATER=	uppercase-value
later=	lowercase-value
# expect+1: Unknown modifier "s,value,replaced,"
.if ${indirect} != "uppercase-replaced ok uppercase-sysv"
# expect+1: warning: XXX Neither branch should be taken.
.  warning	XXX Neither branch should be taken.
.else
.  error
.endif


all:
	@:;
