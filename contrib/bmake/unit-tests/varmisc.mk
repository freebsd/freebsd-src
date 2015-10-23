# $Id: varmisc.mk,v 1.5 2015/10/12 17:10:48 sjg Exp $
#
# Miscellaneous variable tests.

all: unmatched_var_paren D_true U_true D_false U_false Q_lhs Q_rhs NQ_none

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
