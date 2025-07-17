# $NetBSD: directive-include-guard.mk,v 1.19 2025/04/11 17:21:31 rillig Exp $
#
# Tests for multiple-inclusion guards in makefiles.
#
# A file that is guarded by a multiple-inclusion guard has one of the
# following forms:
#
#	.ifndef GUARD_VARIABLE
#	.endif
#
#	.if !defined(GUARD_VARIABLE)
#	.endif
#
#	.if !target(guard-target)
#	.endif
#
# When such a file is included for the second or later time, and the guard
# variable or the guard target is defined, the file is skipped completely, as
# including it would not have any effect, not even on the special variable
# '.MAKE.MAKEFILES', as that variable skips duplicate pathnames.
#
# See also:
#	https://gcc.gnu.org/onlinedocs/cppinternals/Guard-Macros.html

# Each of the following test cases creates a temporary file named after the
# test case and writes some lines of text to that file.  That file is then
# included twice, to see whether the second '.include' is skipped.


# This is the canonical form of a variable-based multiple-inclusion guard.
CASES+=	variable-ifndef
LINES.variable-ifndef= \
	'.ifndef VARIABLE_IFNDEF' \
	'VARIABLE_IFNDEF=' \
	'.endif'
# expect: Parse_PushInput: variable-ifndef.tmp:1
# expect: Skipping 'variable-ifndef.tmp' because 'VARIABLE_IFNDEF' is defined

# A file that reuses a guard from a previous file (or whose guard is defined
# for any other reason) is only processed once, to see whether it is guarded.
# Its content is skipped, therefore the syntax error is not detected.
CASES+=	variable-ifndef-reuse
LINES.variable-ifndef-reuse= \
	'.ifndef VARIABLE_IFNDEF' \
	'syntax error' \
	'.endif'
# expect: Parse_PushInput: variable-ifndef-reuse.tmp:1
# expect: Skipping 'variable-ifndef-reuse.tmp' because 'VARIABLE_IFNDEF' is defined

# The guard variable cannot be a number, as numbers are interpreted
# differently from bare words.
CASES+=	variable-ifndef-zero
LINES.variable-ifndef-zero= \
	'.ifndef 0e0' \
	'syntax error' \
	'.endif'
# expect: Parse_PushInput: variable-ifndef-zero.tmp:1
# expect: Parse_PushInput: variable-ifndef-zero.tmp:1

# The guard variable cannot be a number, as numbers are interpreted
# differently from bare words.
CASES+=	variable-ifndef-one
LINES.variable-ifndef-one= \
	'.ifndef 1' \
	'.endif'
# expect: Parse_PushInput: variable-ifndef-one.tmp:1
# expect: Parse_PushInput: variable-ifndef-one.tmp:1

# Comments and empty lines do not affect the multiple-inclusion guard.
CASES+=	comments
LINES.comments= \
	'\# comment' \
	'' \
	'.ifndef COMMENTS' \
	'\# comment' \
	'COMMENTS=\#comment' \
	'.endif' \
	'\# comment'
# expect: Parse_PushInput: comments.tmp:1
# expect: Skipping 'comments.tmp' because 'COMMENTS' is defined

# An alternative form uses the 'defined' function.  It is more verbose than
# the canonical form but avoids the '.ifndef' directive, as that directive is
# not commonly used.
CASES+=	variable-if
LINES.variable-if= \
	'.if !defined(VARIABLE_IF)' \
	'VARIABLE_IF=' \
	'.endif'
# expect: Parse_PushInput: variable-if.tmp:1
# expect: Skipping 'variable-if.tmp' because 'VARIABLE_IF' is defined

# A file that reuses a guard from a previous file (or whose guard is defined
# for any other reason) is only processed once, to see whether it is guarded.
# Its content is skipped, therefore the syntax error is not detected.
CASES+=	variable-if-reuse
LINES.variable-if-reuse= \
	'.if !defined(VARIABLE_IF)' \
	'syntax error' \
	'.endif'
# expect: Parse_PushInput: variable-if-reuse.tmp:1
# expect: Skipping 'variable-if-reuse.tmp' because 'VARIABLE_IF' is defined

# Triple negation is so uncommon that it's not recognized, even though it has
# the same effect as a single negation.
CASES+=	variable-if-triple-negation
LINES.variable-if-triple-negation= \
	'.if !!!defined(VARIABLE_IF_TRIPLE_NEGATION)' \
	'VARIABLE_IF_TRIPLE_NEGATION=' \
	'.endif'
# expect: Parse_PushInput: variable-if-triple-negation.tmp:1
# expect: Parse_PushInput: variable-if-triple-negation.tmp:1

# If the guard variable is enclosed in spaces, it does not have an effect, as
# that form is not common in practice.
CASES+=	variable-if-spaced
LINES.variable-if-spaced= \
	'.if !defined( VARIABLE_IF_SPACED )' \
	'VARIABLE_IF_SPACED=' \
	'.endif'
# expect: Parse_PushInput: variable-if-spaced.tmp:1
# expect: Parse_PushInput: variable-if-spaced.tmp:1

# If the guard variable condition is enclosed in parentheses, it does not have
# an effect, as that form is not common in practice.
CASES+=	variable-if-parenthesized
LINES.variable-if-parenthesized= \
	'.if (!defined(VARIABLE_IF_PARENTHESIZED))' \
	'VARIABLE_IF_PARENTHESIZED=' \
	'.endif'
# expect: Parse_PushInput: variable-if-parenthesized.tmp:1
# expect: Parse_PushInput: variable-if-parenthesized.tmp:1

# A conditional other than '.if' or '.ifndef' does not guard the file, even if
# it is otherwise equivalent to the above accepted forms.
CASES+=	variable-ifdef-negated
LINES.variable-ifdef-negated= \
	'.ifdef !VARIABLE_IFDEF_NEGATED' \
	'VARIABLE_IFDEF_NEGATED=' \
	'.endif'
# expect: Parse_PushInput: variable-ifdef-negated.tmp:1
# expect: Parse_PushInput: variable-ifdef-negated.tmp:1

# The variable names in the '.if' and the assignment must be the same.
CASES+=	variable-name-mismatch
LINES.variable-name-mismatch= \
	'.ifndef VARIABLE_NAME_MISMATCH' \
	'VARIABLE_NAME_DIFFERENT=' \
	'.endif'
# expect: Parse_PushInput: variable-name-mismatch.tmp:1
# expect: Parse_PushInput: variable-name-mismatch.tmp:1

# If the guard variable condition is enclosed in parentheses, it does not have
# an effect, as that form is not common in practice.
CASES+=	variable-ifndef-parenthesized
LINES.variable-ifndef-parenthesized= \
	'.ifndef (VARIABLE_IFNDEF_PARENTHESIZED)' \
	'VARIABLE_IFNDEF_PARENTHESIZED=' \
	'.endif'
# expect: Parse_PushInput: variable-ifndef-parenthesized.tmp:1
# expect: Parse_PushInput: variable-ifndef-parenthesized.tmp:1

# The variable name '!VARNAME' cannot be used in an '.ifndef' directive, as
# the '!' would be a negation.  It is syntactically valid in a '.if !defined'
# condition, but this case is so uncommon that the guard mechanism doesn't
# accept '!' in the guard variable name. Furthermore, when defining the
# variable, the character '!' has to be escaped, to prevent it from being
# interpreted as the '!' dependency operator.
CASES+=	variable-name-exclamation
LINES.variable-name-exclamation= \
	'.if !defined(!VARIABLE_NAME_EXCLAMATION)' \
	'${:U!}VARIABLE_NAME_EXCLAMATION=' \
	'.endif'
# expect: Parse_PushInput: variable-name-exclamation.tmp:1
# expect: Parse_PushInput: variable-name-exclamation.tmp:1

# In general, a variable name can contain a '!' in the middle, as that
# character is interpreted as an ordinary character in conditions as well as
# on the left side of a variable assignment.  For guard variable names, the
# '!' is not supported in any place, though.
CASES+=	variable-name-exclamation-middle
LINES.variable-name-exclamation-middle= \
	'.ifndef VARIABLE_NAME!MIDDLE' \
	'VARIABLE_NAME!MIDDLE=' \
	'.endif'
# expect: Parse_PushInput: variable-name-exclamation-middle.tmp:1
# expect: Parse_PushInput: variable-name-exclamation-middle.tmp:1

# A variable name can contain balanced parentheses, at least in conditions and
# on the left side of a variable assignment.  There are enough places in make
# where parentheses or braces are handled inconsistently to make this naming
# choice a bad idea, therefore these characters are not allowed in guard
# variable names.
CASES+=	variable-name-parentheses
LINES.variable-name-parentheses= \
	'.ifndef VARIABLE_NAME(&)PARENTHESES' \
	'VARIABLE_NAME(&)PARENTHESES=' \
	'.endif'
# expect: Parse_PushInput: variable-name-parentheses.tmp:1
# expect: Parse_PushInput: variable-name-parentheses.tmp:1

# The guard condition must consist of only the guard variable, nothing else.
CASES+=	variable-ifndef-plus
LINES.variable-ifndef-plus= \
	'.ifndef VARIABLE_IFNDEF_PLUS && VARIABLE_IFNDEF_SECOND' \
	'VARIABLE_IFNDEF_PLUS=' \
	'VARIABLE_IFNDEF_SECOND=' \
	'.endif'
# expect: Parse_PushInput: variable-ifndef-plus.tmp:1
# expect: Parse_PushInput: variable-ifndef-plus.tmp:1

# The guard condition must consist of only the guard variable, nothing else.
CASES+=	variable-if-plus
LINES.variable-if-plus= \
	'.if !defined(VARIABLE_IF_PLUS) && !defined(VARIABLE_IF_SECOND)' \
	'VARIABLE_IF_PLUS=' \
	'VARIABLE_IF_SECOND=' \
	'.endif'
# expect: Parse_PushInput: variable-if-plus.tmp:1
# expect: Parse_PushInput: variable-if-plus.tmp:1

# The variable name in an '.ifndef' guard must be given directly, it must not
# contain any '$' expression.
CASES+=	variable-ifndef-indirect
LINES.variable-ifndef-indirect= \
	'.ifndef $${VARIABLE_IFNDEF_INDIRECT:L}' \
	'VARIABLE_IFNDEF_INDIRECT=' \
	'.endif'
# expect: Parse_PushInput: variable-ifndef-indirect.tmp:1
# expect: Parse_PushInput: variable-ifndef-indirect.tmp:1

# The variable name in an '.if' guard must be given directly, it must not
# contain any '$' expression.
CASES+=	variable-if-indirect
LINES.variable-if-indirect= \
	'.if !defined($${VARIABLE_IF_INDIRECT:L})' \
	'VARIABLE_IF_INDIRECT=' \
	'.endif'
# expect: Parse_PushInput: variable-if-indirect.tmp:1
# expect: Parse_PushInput: variable-if-indirect.tmp:1

# The variable name in the guard condition must only contain alphanumeric
# characters and underscores.  The place where the guard variable is defined
# is more flexible, as long as the variable is defined at the point where the
# file is included the next time.
CASES+=	variable-assign-indirect
LINES.variable-assign-indirect= \
	'.ifndef VARIABLE_ASSIGN_INDIRECT' \
	'$${VARIABLE_ASSIGN_INDIRECT:L}=' \
	'.endif'
# expect: Parse_PushInput: variable-assign-indirect.tmp:1
# expect: Skipping 'variable-assign-indirect.tmp' because 'VARIABLE_ASSIGN_INDIRECT' is defined

# The time at which the guard variable is defined doesn't matter, as long as
# it is defined at the point where the file is included the next time.
CASES+=	variable-assign-late
LINES.variable-assign-late= \
	'.ifndef VARIABLE_ASSIGN_LATE' \
	'VARIABLE_ASSIGN_LATE_OTHER=' \
	'VARIABLE_ASSIGN_LATE=' \
	'.endif'
# expect: Parse_PushInput: variable-assign-late.tmp:1
# expect: Skipping 'variable-assign-late.tmp' because 'VARIABLE_ASSIGN_LATE' is defined

# The time at which the guard variable is defined doesn't matter, as long as
# it is defined at the point where the file is included the next time.
CASES+=	variable-assign-nested
LINES.variable-assign-nested= \
	'.ifndef VARIABLE_ASSIGN_NESTED' \
	'.  if 1' \
	'.    for i in once' \
	'VARIABLE_ASSIGN_NESTED=' \
	'.    endfor' \
	'.  endif' \
	'.endif'
# expect: Parse_PushInput: variable-assign-nested.tmp:1
# expect: Skipping 'variable-assign-nested.tmp' because 'VARIABLE_ASSIGN_NESTED' is defined

# If the guard variable is defined before the file is included for the first
# time, the file is considered guarded as well.  In such a case, the parser
# skips almost all lines, as they are irrelevant, but the structure of the
# top-level '.if/.endif' conditional can be determined reliably enough to
# decide whether the file is guarded.
CASES+=	variable-already-defined
LINES.variable-already-defined= \
	'.ifndef VARIABLE_ALREADY_DEFINED' \
	'VARIABLE_ALREADY_DEFINED=' \
	'.endif'
VARIABLE_ALREADY_DEFINED=
# expect: Parse_PushInput: variable-already-defined.tmp:1
# expect: Skipping 'variable-already-defined.tmp' because 'VARIABLE_ALREADY_DEFINED' is defined

# If the guard variable is defined before the file is included the first time,
# the file is processed but its content is skipped.  If that same guard
# variable is undefined when the file is included the second time, the file is
# processed as usual.
CASES+=	variable-defined-then-undefined
LINES.variable-defined-then-undefined= \
	'.ifndef VARIABLE_DEFINED_THEN_UNDEFINED' \
	'.endif'
VARIABLE_DEFINED_THEN_UNDEFINED=
UNDEF_BETWEEN.variable-defined-then-undefined= \
	VARIABLE_DEFINED_THEN_UNDEFINED
# expect: Parse_PushInput: variable-defined-then-undefined.tmp:1
# expect: Parse_PushInput: variable-defined-then-undefined.tmp:1

# The whole file content must be guarded by a single '.if' conditional, not by
# several, as each of these conditionals would require its separate guard.
# This case is not expected to occur in practice, as the two parts would
# rather be split into separate files.
CASES+=	variable-two-times
LINES.variable-two-times= \
	'.ifndef VARIABLE_TWO_TIMES_1' \
	'VARIABLE_TWO_TIMES_1=' \
	'.endif' \
	'.ifndef VARIABLE_TWO_TIMES_2' \
	'VARIABLE_TWO_TIMES_2=' \
	'.endif'
# expect: Parse_PushInput: variable-two-times.tmp:1
# expect: Parse_PushInput: variable-two-times.tmp:1

# When multiple files use the same guard variable name, the optimization of
# skipping the file affects each of these files.
#
# Choosing unique guard names is the responsibility of the makefile authors.
# A typical pattern of guard variable names is '${PROJECT}_${DIR}_${FILE}_MK'.
# System-provided files typically start the guard names with '_'.
CASES+=	variable-clash
LINES.variable-clash= \
	${LINES.variable-if}
# expect: Parse_PushInput: variable-clash.tmp:1
# expect: Skipping 'variable-clash.tmp' because 'VARIABLE_IF' is defined

# The conditional must come before the assignment, otherwise the conditional
# is useless, as it always evaluates to false.
CASES+=	variable-swapped
LINES.variable-swapped= \
	'SWAPPED=' \
	'.ifndef SWAPPED' \
	'.  error' \
	'.endif'
# expect: Parse_PushInput: variable-swapped.tmp:1
# expect: Parse_PushInput: variable-swapped.tmp:1

# If the guard variable is undefined between the first and the second time the
# file is included, the guarded file is included again.
CASES+=	variable-undef-between
LINES.variable-undef-between= \
	'.ifndef VARIABLE_UNDEF_BETWEEN' \
	'VARIABLE_UNDEF_BETWEEN=' \
	'.endif'
UNDEF_BETWEEN.variable-undef-between= \
	VARIABLE_UNDEF_BETWEEN
# expect: Parse_PushInput: variable-undef-between.tmp:1
# expect: Parse_PushInput: variable-undef-between.tmp:1

# If the guard variable is undefined while the file is included the first
# time, the guard does not have an effect, and the file is included again.
CASES+=	variable-undef-inside
LINES.variable-undef-inside= \
	'.ifndef VARIABLE_UNDEF_INSIDE' \
	'VARIABLE_UNDEF_INSIDE=' \
	'.undef VARIABLE_UNDEF_INSIDE' \
	'.endif'
# expect: Parse_PushInput: variable-undef-inside.tmp:1
# expect: Parse_PushInput: variable-undef-inside.tmp:1

# If the file does not define the guard variable, the guard does not have an
# effect, and the file is included again.
CASES+=	variable-not-defined
LINES.variable-not-defined= \
	'.ifndef VARIABLE_NOT_DEFINED' \
	'.endif'
# expect: Parse_PushInput: variable-not-defined.tmp:1
# expect: Parse_PushInput: variable-not-defined.tmp:1

# The outermost '.if' must not have an '.elif' branch.
CASES+=	elif
LINES.elif= \
	'.ifndef ELIF' \
	'ELIF=' \
	'.elif 1' \
	'.endif'
# expect: Parse_PushInput: elif.tmp:1
# expect: Parse_PushInput: elif.tmp:1

# When a file with an '.if/.elif/.endif' conditional at the top level is
# included, it is never optimized, as one of its branches is taken.
CASES+=	elif-reuse
LINES.elif-reuse= \
	'.ifndef ELIF' \
	'syntax error' \
	'.elif 1' \
	'.endif'
# expect: Parse_PushInput: elif-reuse.tmp:1
# expect: Parse_PushInput: elif-reuse.tmp:1

# The outermost '.if' must not have an '.else' branch.
CASES+=	else
LINES.else= \
	'.ifndef ELSE' \
	'ELSE=' \
	'.else' \
	'.endif'
# expect: Parse_PushInput: else.tmp:1
# expect: Parse_PushInput: else.tmp:1

# When a file with an '.if/.else/.endif' conditional at the top level is
# included, it is never optimized, as one of its branches is taken.
CASES+=	else-reuse
LINES.else-reuse= \
	'.ifndef ELSE' \
	'syntax error' \
	'.else' \
	'.endif'
# expect: Parse_PushInput: else-reuse.tmp:1
# expect: Parse_PushInput: else-reuse.tmp:1

# The inner '.if' directives may have an '.elif' or '.else', and it doesn't
# matter which of their branches are taken.
CASES+=	inner-if-elif-else
LINES.inner-if-elif-else= \
	'.ifndef INNER_IF_ELIF_ELSE' \
	'INNER_IF_ELIF_ELSE=' \
	'.  if 0' \
	'.  elif 0' \
	'.  else' \
	'.  endif' \
	'.  if 0' \
	'.  elif 1' \
	'.  else' \
	'.  endif' \
	'.  if 1' \
	'.  elif 1' \
	'.  else' \
	'.  endif' \
	'.endif'
# expect: Parse_PushInput: inner-if-elif-else.tmp:1
# expect: Skipping 'inner-if-elif-else.tmp' because 'INNER_IF_ELIF_ELSE' is defined

# The guard can also be a target instead of a variable.  Using a target as a
# guard has the benefit that a target cannot be undefined once it is defined.
# The target should be declared '.NOTMAIN'.  Since the target names are
# usually chosen according to a pattern that doesn't interfere with real
# target names, they don't need to be declared '.PHONY' as they don't generate
# filesystem operations.
CASES+=	target
LINES.target= \
	'.if !target(__target.tmp__)' \
	'__target.tmp__: .NOTMAIN' \
	'.endif'
# expect: Parse_PushInput: target.tmp:1
# expect: Skipping 'target.tmp' because '__target.tmp__' is defined

# When used for system files, the target name may include '<' and '>', for
# symmetry with the '.include <sys.mk>' directive.  The characters '<' and '>'
# are ordinary characters.
CASES+=	target-sys
LINES.target-sys= \
	'.if !target(__<target-sys.tmp>__)' \
	'__<target-sys.tmp>__: .NOTMAIN' \
	'.endif'
# expect: Parse_PushInput: target-sys.tmp:1
# expect: Skipping 'target-sys.tmp' because '__<target-sys.tmp>__' is defined

# The target name may include variable references.  These references are
# expanded as usual.  Due to the current implementation, the expressions are
# evaluated twice:  Once for checking whether the condition evaluates to true,
# and once for determining the guard name.  This double evaluation should not
# matter in practice, as guard expressions are expected to be simple,
# deterministic and without side effects.
CASES+=	target-indirect
LINES.target-indirect= \
	'.if !target($${target-indirect.tmp:L})' \
	'target-indirect.tmp: .NOTMAIN' \
	'.endif'
# expect: Parse_PushInput: target-indirect.tmp:1
# expect: Skipping 'target-indirect.tmp' because 'target-indirect.tmp' is defined

# A common form of guard target is __${.PARSEFILE}__.  This form can only be
# used if all files using this form have unique basenames.  To get a robust
# pattern based on the same idea, use __${.PARSEDIR}/${.PARSEFILE}__ instead.
# This form does not work when the basename contains whitespace characters, as
# it is not possible to define a target with whitespace, not even by cheating.
CASES+=	target-indirect-PARSEFILE
LINES.target-indirect-PARSEFILE= \
	'.if !target(__$${.PARSEFILE}__)' \
	'__$${.PARSEFILE}__: .NOTMAIN' \
	'.endif'
# expect: Parse_PushInput: target-indirect-PARSEFILE.tmp:1
# expect: Skipping 'target-indirect-PARSEFILE.tmp' because '__target-indirect-PARSEFILE.tmp__' is defined

# Two files with different basenames can both use the same syntactic pattern
# for the target guard name, as the expressions expand to different strings.
CASES+=	target-indirect-PARSEFILE2
LINES.target-indirect-PARSEFILE2= \
	'.if !target(__$${.PARSEFILE}__)' \
	'__$${.PARSEFILE}__: .NOTMAIN' \
	'.endif'
# expect: Parse_PushInput: target-indirect-PARSEFILE2.tmp:1
# expect: Skipping 'target-indirect-PARSEFILE2.tmp' because '__target-indirect-PARSEFILE2.tmp__' is defined

# Using plain .PARSEFILE without .PARSEDIR leads to name clashes.  The include
# guard is the same as in the test case 'target-indirect-PARSEFILE', as the
# guard name only contains the basename but not the directory name.  So even
# without defining the guard target, the file is considered guarded.
CASES+=	subdir/target-indirect-PARSEFILE
LINES.subdir/target-indirect-PARSEFILE= \
	'.if !target(__$${.PARSEFILE}__)' \
	'.endif'
# expect: Parse_PushInput: subdir/target-indirect-PARSEFILE.tmp:1
# expect: Skipping 'subdir/target-indirect-PARSEFILE.tmp' because '__target-indirect-PARSEFILE.tmp__' is defined

# Another common form of guard target is __${.PARSEDIR}/${.PARSEFILE}__
# or __${.PARSEDIR:tA}/${.PARSEFILE}__ to be truly unique.
CASES+=	target-indirect-PARSEDIR-PARSEFILE
LINES.target-indirect-PARSEDIR-PARSEFILE= \
	'.if !target(__$${.PARSEDIR}/$${.PARSEFILE}__)' \
	'__$${.PARSEDIR}/$${.PARSEFILE}__: .NOTMAIN' \
	'.endif'
# expect: Parse_PushInput: target-indirect-PARSEDIR-PARSEFILE.tmp:1
# expect: Skipping 'target-indirect-PARSEDIR-PARSEFILE.tmp' because '__target-indirect-PARSEDIR-PARSEFILE.tmp__' is defined
# The actual target starts with '__${.OBJDIR}/', see the .rawout file, but the
# string '${.OBJDIR}/' gets stripped in post processing.

# Using the combination of '.PARSEDIR' and '.PARSEFILE', a file in a
# subdirectory gets a different guard target name than the previous one.
CASES+=	subdir/target-indirect-PARSEDIR-PARSEFILE
LINES.subdir/target-indirect-PARSEDIR-PARSEFILE= \
	'.if !target(__$${.PARSEDIR}/$${.PARSEFILE}__)' \
	'__$${.PARSEDIR}/$${.PARSEFILE}__: .NOTMAIN' \
	'.endif'
# expect: Parse_PushInput: subdir/target-indirect-PARSEDIR-PARSEFILE.tmp:1
# expect: Skipping 'subdir/target-indirect-PARSEDIR-PARSEFILE.tmp' because '__subdir/target-indirect-PARSEDIR-PARSEFILE.tmp__' is defined
# The actual target starts with '__${.OBJDIR}/', see the .rawout file, but the
# string '${.OBJDIR}/' gets stripped in post processing.

# If the guard target is not defined when including the file the next time,
# the file is processed again.
CASES+=	target-unguarded
LINES.target-unguarded= \
	'.if !target(target-unguarded)' \
	'.endif'
# expect: Parse_PushInput: target-unguarded.tmp:1
# expect: Parse_PushInput: target-unguarded.tmp:1

# The guard condition must consist of only the guard target, nothing else.
CASES+=	target-plus
LINES.target-plus= \
	'.if !target(target-plus) && 1' \
	'target-plus: .NOTMAIN' \
	'.endif'
# expect: Parse_PushInput: target-plus.tmp:1
# expect: Parse_PushInput: target-plus.tmp:1

# If the guard target is defined before the file is included the first time,
# the file is read once and then considered guarded.
CASES+=	target-already-defined
LINES.target-already-defined= \
	'.if !target(target-already-defined)' \
	'target-already-defined: .NOTMAIN' \
	'.endif'
target-already-defined: .NOTMAIN
# expect: Parse_PushInput: target-already-defined.tmp:1
# expect: Skipping 'target-already-defined.tmp' because 'target-already-defined' is defined

# A target name cannot contain the character '!'.  In the condition, the '!'
# is syntactically valid, but in the dependency declaration line, the '!' is
# interpreted as the '!' dependency operator, no matter whether it occurs at
# the beginning or in the middle of a target name.  Escaping it as '${:U!}'
# doesn't work, as the whole line is first expanded and then scanned for the
# dependency operator.  Escaping it as '\!' doesn't work either, even though
# the '\' escapes the '!' from being a dependency operator, but when reading
# the target name, the '\' is kept, resulting in the target name
# '\!target-name-exclamation' instead of '!target-name-exclamation'.
CASES+=	target-name-exclamation
LINES.target-name-exclamation= \
	'.if !target(!target-name-exclamation)' \
	'\!target-name-exclamation: .NOTMAIN' \
	'.endif'
# expect: Parse_PushInput: target-name-exclamation.tmp:1
# expect: Parse_PushInput: target-name-exclamation.tmp:1

# If the guard target name has leading spaces, it does not have an effect,
# as that form is not common in practice.
CASES+=	target-name-leading-space
LINES.target-name-leading-space= \
	'.if !target( target-name-leading-space)' \
	'target-name-leading-space: .NOTMAIN' \
	'.endif'
# expect: Parse_PushInput: target-name-leading-space.tmp:1
# expect: Parse_PushInput: target-name-leading-space.tmp:1

# If the guard target name has trailing spaces, it does not have an effect,
# as that form is not common in practice.
CASES+=	target-name-trailing-space
LINES.target-name-trailing-space= \
	'.if !target(target-name-trailing-space )' \
	'target-name-trailing-space: .NOTMAIN' \
	'.endif'
# expect: Parse_PushInput: target-name-trailing-space.tmp:1
# expect: Parse_PushInput: target-name-trailing-space.tmp:1

# If the guard target condition is enclosed in parentheses, it does not have
# an effect, as that form is not common in practice.
CASES+=	target-call-parenthesized
LINES.target-call-parenthesized= \
	'.if (!target(target-call-parenthesized))' \
	'target-call-parenthesized: .NOTMAIN' \
	'.endif'
# expect: Parse_PushInput: target-call-parenthesized.tmp:1
# expect: Parse_PushInput: target-call-parenthesized.tmp:1

# If the '.if' or '.ifndef' directive spans more than a single line, it is
# still recognized as a guard condition.  This case is entirely uncommon, but
# at the point where the guard condition is checked, line continuations have
# already been converted to spaces.
CASES+=	multiline
LINES.multiline= \
	'.\' \
	'  ifndef \' \
	'  MULTILINE' \
	'MULTILINE=' \
	'.endif'
# expect: Parse_PushInput: multiline.tmp:1
# expect: Skipping 'multiline.tmp' because 'MULTILINE' is defined


# Now run all test cases by including each of the files twice and looking at
# the debug output.  The files that properly guard against multiple inclusion
# generate a 'Skipping' line, the others repeat the 'Parse_PushInput' line.
#
# Some debug output lines are suppressed in the .exp file, see ./Makefile.
.for i in ${CASES}
.  for fname in $i.tmp
_:=	${fname:H:N.:@dir@${:!mkdir -p ${dir}!}@}
_!=	printf '%s\n' ${LINES.$i} > ${fname}
.MAKEFLAGS: -dp
.include "${.CURDIR}/${fname}"
.undef ${UNDEF_BETWEEN.$i:U}
.include "${.CURDIR}/${fname}"
.MAKEFLAGS: -d0
_!=	rm ${fname}
_:=	${fname:H:N.:@dir@${:!rmdir ${dir}!}@}
.  endfor
.endfor

all:
