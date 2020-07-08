# $Id: varmisc.mk,v 1.11 2020/07/02 15:43:43 sjg Exp $
#
# Miscellaneous variable tests.

all: unmatched_var_paren D_true U_true D_false U_false Q_lhs Q_rhs NQ_none \
	strftime cmpv manok

unmatched_var_paren:
	@echo ${foo::=foo-text}

True = ${echo true >&2:L:sh}TRUE
False= ${echo false >&2:L:sh}FALSE

VSET= is set
.undef UNDEF

U_false:
	@echo :U skipped when var set
	@echo ${VSET:U${False}}

D_false:
	@echo :D skipped if var undef
	@echo ${UNDEF:D${False}}

U_true:
	@echo :U expanded when var undef
	@echo ${UNDEF:U${True}}

D_true:
	@echo :D expanded when var set
	@echo ${VSET:D${True}}

Q_lhs:
	@echo :? only lhs when value true
	@echo ${1:L:?${True}:${False}}

Q_rhs:
	@echo :? only rhs when value false
	@echo ${0:L:?${True}:${False}}

NQ_none:
	@echo do not evaluate or expand :? if discarding
	@echo ${VSET:U${1:L:?${True}:${False}}}

April1= 1459494000

# slightly contorted syntax to use utc via variable
strftime:
	@echo ${year=%Y month=%m day=%d:L:gmtime=1459494000}
	@echo date=${%Y%m%d:L:${gmtime=${April1}:L}}

# big jumps to handle 3 digits per step
M_cmpv.units = 1 1000 1000000
M_cmpv = S,., ,g:_:range:@i@+ $${_:[-$$i]} \* $${M_cmpv.units:[$$i]}@:S,^,expr 0 ,1:sh

Version = 123.456.789
cmpv.only = target specific vars

cmpv:
	@echo Version=${Version} == ${Version:${M_cmpv}}
	@echo Literal=3.4.5 == ${3.4.5:L:${M_cmpv}}
	@echo We have ${${.TARGET:T}.only}

# catch misshandling of nested vars in .for loop
MAN=
MAN1= make.1
.for s in 1 2
.if defined(MAN$s) && !empty(MAN$s)
MAN+= ${MAN$s}
.endif
.endfor

manok:
	@echo MAN=${MAN}

# This is an expanded variant of the above .for loop.
# Between 2020-08-28 and 2020-07-02 this paragraph generated a wrong
# error message "Variable VARNAME is recursive".
# When evaluating the !empty expression, the ${:U1} was not expanded and
# thus resulted in the seeming definition VARNAME=${VARNAME}, which is
# obviously recursive.
VARNAME=	${VARNAME${:U1}}
.if defined(VARNAME${:U2}) && !empty(VARNAME${:U2})
.endif
