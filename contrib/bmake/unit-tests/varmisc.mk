# $NetBSD: varmisc.mk,v 1.37 2024/08/29 20:20:36 rillig Exp $
#
# Miscellaneous variable tests.

all: unmatched_var_paren D_true U_true D_false U_false Q_lhs Q_rhs NQ_none \
	cmpv
all: save-dollars
all: export-appended
all: parse-dynamic
all: varerror-unclosed-{1,2,3,4,5,6,7,8}

unmatched_var_paren:
	@echo ${foo::=foo-text}

True=	${echo true >&2:L:sh}TRUE
False=	${echo false >&2:L:sh}FALSE

VSET=	is set
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

# big jumps to handle 3 digits per step
M_cmpv.units=	1 1000 1000000
M_cmpv=		S,., ,g:_:range:@i@+ $${_:[-$$i]} \* $${M_cmpv.units:[$$i]}@:S,^,expr 0 ,1:sh

Version=	123.456.789
cmpv.only=	target specific vars

cmpv:
	@echo Version=${Version} == ${Version:${M_cmpv}}
	@echo Literal=3.4.5 == ${3.4.5:L:${M_cmpv}}
	@echo We have ${${.TARGET:T}.only}


# Test parsing of boolean values.
# begin .MAKE.SAVE_DOLLARS; see Var_SetWithFlags and ParseBoolean.
SD_VALUES=	0 1 2 False True false true Yes No yes no On Off ON OFF on off
SD_4_DOLLARS=	$$$$

.for val in ${SD_VALUES}
# The assignment must be done using ':=' since a simple '=' would be
# interpreted as 'yes', due to the leading '$'; see ParseBoolean.
.MAKE.SAVE_DOLLARS:=	${val}
SD.${val}:=		${SD_4_DOLLARS}
.endfor
.MAKE.SAVE_DOLLARS:=	yes

save-dollars:
.for val in ${SD_VALUES}
	@printf '%s: %-8s = %s\n' $@ ${val} ${SD.${val}:Q}
.endfor
# end .MAKE.SAVE_DOLLARS

# Appending to an undefined variable does not add a space in front.
.undef APPENDED
APPENDED+=	value
.if ${APPENDED} != "value"
.  error "${APPENDED}"
.endif

# Appending to an empty variable adds a space between the old value
# and the additional value.
APPENDED=	# empty
APPENDED+=	value
.if ${APPENDED} != " value"
.  error "${APPENDED}"
.endif

# Appending to parameterized variables works as well.
PARAM=		param
VAR.${PARAM}=	1
VAR.${PARAM}+=	2
.if ${VAR.param} != "1 2"
.  error "${VAR.param}"
.endif

# The variable name can contain arbitrary characters.
# If the expanded variable name ends in a +, this still does not influence
# the parser. The assignment operator is still a simple assignment.
# Therefore, there is no need to add a space between the variable name
# and the assignment operator.
PARAM=		+
VAR.${PARAM}=	1
VAR.${PARAM}+=	2
.if ${VAR.+} != "1 2"
.  error "${VAR.+}"
.endif
.for param in : + ! ?
VAR.${param}=	${param}
.endfor
.if ${VAR.${:U\:}} != ":" || ${VAR.+} != "+" || ${VAR.!} != "!" || ${VAR.?} != "?"
.  error "${VAR.+}" "${VAR.!}" "${VAR.?}"
.endif

# Appending to a variable from the environment creates a copy of that variable
# in the global scope.
# The appended value is not exported automatically.
# When a variable is exported, the exported value is taken at the time of the
# .export directive. Later changes to the variable have no effect.
.export FROM_ENV_BEFORE
FROM_ENV+=		mk
FROM_ENV_BEFORE+=	mk
FROM_ENV_AFTER+=	mk
.export FROM_ENV_AFTER

export-appended:
	@echo $@: "$$FROM_ENV"
	@echo $@: "$$FROM_ENV_BEFORE"
	@echo $@: "$$FROM_ENV_AFTER"

# begin parse-dynamic
#
# Demonstrate that the target-specific variables are not evaluated in
# the global scope. Their expressions are preserved until there is a local
# scope in which resolving them makes sense.

# There are different code paths for short names ...
${:U>}=		before
GS_TARGET:=	$@
GS_MEMBER:=	$%
GS_PREFIX:=	$*
GS_ARCHIVE:=	$!
GS_ALLSRC:=	$>
${:U>}=		after
# ... and for braced short names ...
GB_TARGET:=	${@}
GB_MEMBER:=	${%}
GB_PREFIX:=	${*}
GB_ARCHIVE:=	${!}
GB_ALLSRC:=	${>}
# ... and for long names.
GL_TARGET:=	${.TARGET}
GL_MEMBER:=	${.MEMBER}
GL_PREFIX:=	${.PREFIX}
GL_ARCHIVE:=	${.ARCHIVE}
GL_ALLSRC:=	${.ALLSRC}

parse-dynamic:
	@echo $@: ${GS_TARGET} ${GS_MEMBER} ${GS_PREFIX} ${GS_ARCHIVE} ${GS_ALLSRC}
	@echo $@: ${GB_TARGET} ${GB_MEMBER} ${GB_PREFIX} ${GB_ARCHIVE} ${GB_ALLSRC}
	@echo $@: ${GL_TARGET} ${GL_MEMBER} ${GL_PREFIX} ${GL_ARCHIVE} ${GL_ALLSRC}

# Since 2020-07-28, make complains about unclosed variables.
# Before that, it had complained about unclosed variables only when
# parsing the modifiers, but not when parsing the variable name.

UNCLOSED_INDIR_1=	${UNCLOSED_ORIG
UNCLOSED_INDIR_2=	${UNCLOSED_INDIR_1}

FLAGS=	one two
FLAGS+=	${FLAGS.${.ALLSRC:M*.c:T:u}}
FLAGS.target2.c= three four

target1.c:
target2.c:

all: target1-flags target2-flags
target1-flags: target1.c
	@echo $@: we have: ${FLAGS}

target2-flags: target2.c
	@echo $@: we have: ${FLAGS}

varerror-unclosed-1:
	@echo $@:begin
varerror-unclosed-2:
# expect: make: Unclosed variable ""
	@echo $(
varerror-unclosed-3:
# expect: make: Unclosed variable "UNCLOSED"
	@echo $(UNCLOSED
varerror-unclosed-4:
# expect: make: Unclosed variable "UNCLOSED"
	@echo ${UNCLOSED
varerror-unclosed-5:
# expect: make: Unclosed expression, expecting '}' for modifier "M${PATTERN"
	@echo ${UNCLOSED:M${PATTERN
varerror-unclosed-6:
# expect: make: Unclosed variable "param"
# expect: make: Unclosed variable "UNCLOSED."
	@echo ${UNCLOSED.${param
varerror-unclosed-7:
	@echo $
.for i in 1 2 3
# expect: make: Unclosed variable "UNCLOSED.1"
	@echo ${UNCLOSED.${i}
.endfor
varerror-unclosed-8:
	@echo ${UNCLOSED_INDIR_2}
	@echo $@:end
