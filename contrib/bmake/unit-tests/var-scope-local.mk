# $NetBSD: var-scope-local.mk,v 1.11 2024/03/05 23:07:58 rillig Exp $
#
# Tests for target-local variables, such as ${.TARGET} or $@.  These variables
# are relatively short-lived as they are created just before making the
# target.  In contrast, global variables are typically created when the
# makefiles are read in.
#
# The 7 built-in target-local variables are listed in the manual page.  They
# are defined just before the target is actually made.  Additional
# target-local variables can be defined in dependency lines like
# 'target: VAR=value', one at a time.

.MAIN: all

# Target-local variables in a target rule
#
# In target rules, '$*' only strips the extension off the pathname if the
# extension is listed in '.SUFFIXES'.
#
# expect: target-rule.ext: * = <target-rule.ext>
all: target-rule.ext dir/subdir/target-rule.ext
target-rule.ext dir/subdir/target-rule.ext: .PHONY
	@echo '$@: @ = <${@:Uundefined}>'
	@echo '$@: % = <${%:Uundefined}>'
	@echo '$@: ? = <${?:Uundefined}>'
	@echo '$@: < = <${<:Uundefined}>'
	@echo '$@: * = <${*:Uundefined}>'

.SUFFIXES: .ir-gen-from .ir-from .ir-to

# In target rules, '$*' strips the extension off the pathname of the target
# if the extension is listed in '.SUFFIXES'.
#
# expect: target-rule.ir-gen-from: * = <target-rule>
all: target-rule.ir-gen-from dir/subdir/target-rule-dir.ir-gen-from
target-rule.ir-gen-from dir/subdir/target-rule-dir.ir-gen-from:
	@echo '$@: @ = <${@:Uundefined}>'
	@echo '$@: % = <${%:Uundefined}>'
	@echo '$@: ? = <${?:Uundefined}>'
	@echo '$@: < = <${<:Uundefined}>'
	@echo '$@: * = <${*:Uundefined}>'

.ir-from.ir-to:
	@echo '$@: @ = <${@:Uundefined}>'
	@echo '$@: % = <${%:Uundefined}>'
	@echo '$@: ? = <${?:Uundefined}>'
	@echo '$@: < = <${<:Uundefined}>'
	@echo '$@: * = <${*:Uundefined}>'
.ir-gen-from.ir-from:
	@echo '$@: @ = <${@:Uundefined}>'
	@echo '$@: % = <${%:Uundefined}>'
	@echo '$@: ? = <${?:Uundefined}>'
	@echo '$@: < = <${<:Uundefined}>'
	@echo '$@: * = <${*:Uundefined}>'

# Target-local variables in an inference rule
all: inference-rule.ir-to dir/subdir/inference-rule.ir-to
inference-rule.ir-from: .PHONY
dir/subdir/inference-rule.ir-from: .PHONY

# Target-local variables in a chain of inference rules
all: inference-rule-chain.ir-to dir/subdir/inference-rule-chain.ir-to
inference-rule-chain.ir-gen-from: .PHONY
dir/subdir/inference-rule-chain.ir-gen-from: .PHONY

# The run-time 'check' directives from above happen after the parse-time
# 'check' directives from below.
#
# expect-reset

# Deferred evaluation during parsing
#
# The target-local variables can be used in expressions, just like other
# variables.  When these expressions are evaluated outside of a target, these
# expressions are not yet expanded, instead their text is preserved, to allow
# these expressions to expand right in time when the target-local variables
# are actually set.
#
# Conditions from .if directives are evaluated in the scope of the command
# line, which means that variables from the command line, from the global
# scope and from the environment are resolved, in this precedence order (but
# see the command line option '-e').  In that phase, expressions involving
# target-local variables need to be preserved, including the exact names of
# the variables.
#
# Each of the built-in target-local variables has two equivalent names, for
# example '@' is equivalent to '.TARGET'.  The implementation might
# canonicalize these aliases at some point, and that might be surprising.
# This aliasing happens for single-character variable names like $@ or $<
# (see VarFind, CanonicalVarname), but not for braced or parenthesized
# expressions like ${@}, ${.TARGET} ${VAR:Mpattern} (see Var_Parse,
# ParseVarname).
#
# In the following condition, make expands '$@' to the long-format alias
# '$(.TARGET)'; note that the alias is not written with braces, as would be
# common in BSD makefiles, but with parentheses.  This alternative spelling
# behaves the same though.
.if $@ != "\$\(.TARGET)"
.  error
.endif
# In the long form of writing a target-local variable, the text of the
# expression is preserved exactly as written, no matter whether it is written
# with '{' or '('.
.if ${@} != "\$\{@}"
.  error
.endif
.if $(@) != "\$\(@)"
.  error
.endif
# If the expression contains modifiers, the behavior depends on the
# actual modifiers.  The modifier ':M' keeps the expression in the state
# 'undefined'.  Since the expression is still undefined after evaluating all
# the modifiers, the value of the expression is discarded and the expression
# text is used instead.  This preserves the expressions based on target-local
# variables as long as possible.
.if ${@:M*} != "\$\{@:M*}"
.  error
.endif
# In the following examples, the expressions are based on target-local
# variables but use the modifier ':L', which turns an undefined expression
# into a defined one.  At the end of evaluating the expression, the state of
# the expression is not 'undefined' anymore.  The value of the expression
# is the name of the variable, since that's what the modifier ':L' does.
.if ${@:L} != "@"
.  error
.endif
.if ${.TARGET:L} != ".TARGET"
.  error
.endif
.if ${@F:L} != "@F"
.  error
.endif
.if ${@D:L} != "@D"
.  error
.endif


# Custom local variables
#
# Additional target-local variables may be defined in dependency lines.
.MAKEFLAGS: -dv
# In the following line, the ':=' may either be interpreted as an assignment
# operator or as the dependency operator ':', followed by an empty variable
# name and the assignment operator '='.  It is the latter since in an
# assignment, the left-hand side must be a single word or empty.
#
# The empty variable name is expanded twice, once for 'one' and once for
# 'two'.
# expect: one: ignoring ' = three' as the variable name '' expands to empty
# expect: two: ignoring ' = three' as the variable name '' expands to empty
one two:=three
# If the two targets to the left are generated by an expression, the
# line is parsed as a variable assignment since its left-hand side is a single
# word.
# expect: Global: one two = three
${:Uone two}:=three
.MAKEFLAGS: -d0


.SUFFIXES: .c .o

# One of the dynamic target-local variables is '.TARGET'.  Since this is not
# a suffix transformation rule, the variable '.IMPSRC' is not defined.
# expect: : Making var-scope-local.c out of nothing.
var-scope-local.c:
	: Making ${.TARGET} ${.IMPSRC:Dfrom ${.IMPSRC}:Uout of nothing}.

# This is a suffix transformation rule, so both '.TARGET' and '.IMPSRC' are
# defined.
# expect: : Making var-scope-local.o from var-scope-local.c.
# expect: : Making basename "var-scope-local.o" in "." from "var-scope-local.c" in ".".
.c.o:
	: Making ${.TARGET} from ${.IMPSRC}.

	# The local variables @F, @D, <F, <D are legacy forms.
	# See the manual page for details.
	: Making basename "${@F}" in "${@D}" from "${<F}" in "${<D}".

# expect: : all overwritten
all: var-scope-local.o
	# The ::= modifier overwrites the .TARGET variable in the node
	# 'all', not in the global scope.  This can be seen with the -dv
	# option, looking for "all: @ = overwritten".
	: ${.TARGET} ${.TARGET::=overwritten}${.TARGET}


# Begin tests for custom target-local variables, for all 5 variable assignment
# operators.
all: var-scope-local-assign.o
all: var-scope-local-append.o
all: var-scope-local-append-global.o
all: var-scope-local-default.o
all: var-scope-local-subst.o
all: var-scope-local-shell.o

var-scope-local-assign.o \
var-scope-local-append.o \
var-scope-local-append-global.o \
var-scope-local-default.o \
var-scope-local-subst.o \
var-scope-local-shell.o:
	@echo "Making ${.TARGET} with make '"${VAR:Q}"' and env '$$VAR'."

# Target-local variables are enabled by default.  Force them to be enabled
# just in case a test above has disabled them.
.MAKE.TARGET_LOCAL_VARIABLES= yes

VAR=	global
.export VAR

# If the sources of a dependency line look like a variable assignment, make
# treats them as such.  There is only a single variable assignment per
# dependency line, which makes whitespace around the assignment operator
# irrelevant.
#
# expect-reset
# expect: Making var-scope-local-assign.o with make 'local' and env 'local'.
var-scope-local-assign.o: VAR= local

# Assignments using '+=' do *not* look up the global value, instead they only
# look up the variable in the target's own scope.
var-scope-local-append.o: VAR+= local
# Once a variable is defined in the target-local scope, appending using '+='
# behaves as expected.  Note that the expression '${.TARGET}' is not resolved
# when parsing the dependency line, its evaluation is deferred until the
# target is actually made.
# expect: Making var-scope-local-append.o with make 'local to var-scope-local-append.o' and env 'local to var-scope-local-append.o'.
var-scope-local-append.o: VAR += to ${.TARGET}
# To access the value of a global variable, use an expression.  This
# expression is expanded before parsing the whole dependency line.  Since the
# expansion happens to the right of the dependency operator ':', the expanded
# text does not influence parsing of the dependency line.  Since the expansion
# happens to the right of the assignment operator '=', the expanded text does
# not influence the parsing of the variable assignment.  The effective
# variable assignment, after expanding the whole line first, is thus
# 'VAR= global+local'.
# expect: Making var-scope-local-append-global.o with make 'global+local' and env 'global+local'.
var-scope-local-append-global.o: VAR= ${VAR}+local

var-scope-local-default.o: VAR ?= first
var-scope-local-default.o: VAR ?= second
# XXX: '?=' does look at the global variable.  That's a long-standing
# inconsistency between the assignment operators '+=' and '?='.  See
# Var_AppendExpand and VarAssign_Eval.
# expect: Making var-scope-local-default.o with make 'global' and env 'global'.

# Using the variable assignment operator ':=' provides another way of
# accessing a global variable and extending it with local modifications.  The
# '$' has to be written as '$$' though to survive the expansion of the
# dependency line as a whole.  After that, the parser sees the variable
# assignment as 'VAR := ${VAR}+local' and searches for the variable 'VAR' in
# the usual scopes, picking up the variable from the global scope.
# expect: Making var-scope-local-subst.o with make 'global+local' and env 'global+local'.
var-scope-local-subst.o: VAR := $${VAR}+local

# The variable assignment operator '!=' assigns the output of the shell
# command, as everywhere else.  The shell command is run when the dependency
# line is parsed.
var-scope-local-shell.o: VAR != echo output


# While VAR=use will be set for a .USE node, it will never be seen since only
# the ultimate target's context is searched; the variable assignments from the
# .USE target are not copied to the ultimate target's.
# expect: Making .USE var-scope-local-use.o with make 'global' and env 'global'.
a_use: .USE VAR=use
	@echo "Making .USE ${.TARGET} with make '"${VAR:Q}"' and env '$$VAR'."

all: var-scope-local-use.o
var-scope-local-use.o: a_use
