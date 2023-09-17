# $NetBSD: directive-include-guard.mk,v 1.12 2023/08/11 04:56:31 rillig Exp $
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
# variable or the guard target is defined, including the file has no effect,
# as all its content is skipped.
#
# See also:
#	https://gcc.gnu.org/onlinedocs/cppinternals/Guard-Macros.html

# Each of the following test cases creates a temporary file named after the
# test case and writes some lines of text to that file.  That file is then
# included twice, to see whether the second '.include' is skipped.


# This is the canonical form of a variable-based multiple-inclusion guard.
INCS+=	variable-ifndef
LINES.variable-ifndef= \
	'.ifndef VARIABLE_IFNDEF' \
	'VARIABLE_IFNDEF=' \
	'.endif'
# expect: Parse_PushInput: file variable-ifndef.tmp, line 1
# expect: Skipping 'variable-ifndef.tmp' because 'VARIABLE_IFNDEF' is defined

# A file that reuses a guard from a previous file (or whose guard is defined
# for any other reason) is only processed once, to see whether it is guarded.
# Its content is skipped, therefore the syntax error is not detected.
INCS+=	variable-ifndef-reuse
LINES.variable-ifndef-reuse= \
	'.ifndef VARIABLE_IFNDEF' \
	'syntax error' \
	'.endif'
# expect: Parse_PushInput: file variable-ifndef-reuse.tmp, line 1
# expect: Skipping 'variable-ifndef-reuse.tmp' because 'VARIABLE_IFNDEF' is defined

# Comments and empty lines do not affect the multiple-inclusion guard.
INCS+=	comments
LINES.comments= \
	'\# comment' \
	'' \
	'.ifndef COMMENTS' \
	'\# comment' \
	'COMMENTS=\#comment' \
	'.endif' \
	'\# comment'
# expect: Parse_PushInput: file comments.tmp, line 1
# expect: Skipping 'comments.tmp' because 'COMMENTS' is defined

# An alternative form uses the 'defined' function.  It is more verbose than
# the canonical form but avoids the '.ifndef' directive, as that directive is
# not commonly used.
INCS+=	variable-if
LINES.variable-if= \
	'.if !defined(VARIABLE_IF)' \
	'VARIABLE_IF=' \
	'.endif'
# expect: Parse_PushInput: file variable-if.tmp, line 1
# expect: Skipping 'variable-if.tmp' because 'VARIABLE_IF' is defined

# A file that reuses a guard from a previous file (or whose guard is defined
# for any other reason) is only processed once, to see whether it is guarded.
# Its content is skipped, therefore the syntax error is not detected.
INCS+=	variable-if-reuse
LINES.variable-if-reuse= \
	'.if !defined(VARIABLE_IF)' \
	'syntax error' \
	'.endif'
# expect: Parse_PushInput: file variable-if-reuse.tmp, line 1
# expect: Skipping 'variable-if-reuse.tmp' because 'VARIABLE_IF' is defined

# Triple negation is so uncommon that it's not recognized, even though it has
# the same effect as a single negation.
INCS+=	variable-if-triple-negation
LINES.variable-if-triple-negation= \
	'.if !!!defined(VARIABLE_IF_TRIPLE_NEGATION)' \
	'VARIABLE_IF_TRIPLE_NEGATION=' \
	'.endif'
# expect: Parse_PushInput: file variable-if-triple-negation.tmp, line 1
# expect: Parse_PushInput: file variable-if-triple-negation.tmp, line 1

# A conditional other than '.if' or '.ifndef' does not guard the file, even if
# it is otherwise equivalent to the above accepted forms.
INCS+=	variable-ifdef-negated
LINES.variable-ifdef-negated= \
	'.ifdef !VARIABLE_IFDEF_NEGATED' \
	'VARIABLE_IFDEF_NEGATED=' \
	'.endif'
# expect: Parse_PushInput: file variable-ifdef-negated.tmp, line 1
# expect: Parse_PushInput: file variable-ifdef-negated.tmp, line 1

# The variable names in the '.if' and the assignment must be the same.
INCS+=	variable-name-mismatch
LINES.variable-name-mismatch= \
	'.ifndef VARIABLE_NAME_MISMATCH' \
	'VARIABLE_NAME_DIFFERENT=' \
	'.endif'
# expect: Parse_PushInput: file variable-name-mismatch.tmp, line 1
# expect: Parse_PushInput: file variable-name-mismatch.tmp, line 1

# The variable name '!VARNAME' cannot be used in an '.ifndef' directive, as
# the '!' would be a negation.  It is syntactically valid in a '.if !defined'
# condition, but this case is so uncommon that the guard mechanism doesn't
# accept '!' in the guard variable name. Furthermore, when defining the
# variable, the character '!' has to be escaped, to prevent it from being
# interpreted as the '!' dependency operator.
INCS+=	variable-name-exclamation
LINES.variable-name-exclamation= \
	'.if !defined(!VARIABLE_NAME_EXCLAMATION)' \
	'${:U!}VARIABLE_NAME_EXCLAMATION=' \
	'.endif'
# expect: Parse_PushInput: file variable-name-exclamation.tmp, line 1
# expect: Parse_PushInput: file variable-name-exclamation.tmp, line 1

# A variable name can contain a '!' in the middle, as that character is
# interpreted as an ordinary character in conditions as well as on the left
# side of a variable assignment.  For guard variable names, the '!' is not
# supported in any place, though.
INCS+=	variable-name-exclamation-middle
LINES.variable-name-exclamation-middle= \
	'.ifndef VARIABLE_NAME!MIDDLE' \
	'VARIABLE_NAME!MIDDLE=' \
	'.endif'
# expect: Parse_PushInput: file variable-name-exclamation-middle.tmp, line 1
# expect: Parse_PushInput: file variable-name-exclamation-middle.tmp, line 1

# A variable name can contain balanced parentheses, at least in conditions and
# on the left side of a variable assignment.  There are enough places in make
# where parentheses or braces are handled inconsistently to make this naming
# choice a bad idea, therefore these characters are not allowed in guard
# variable names.
INCS+=	variable-name-parentheses
LINES.variable-name-parentheses= \
	'.ifndef VARIABLE_NAME(&)PARENTHESES' \
	'VARIABLE_NAME(&)PARENTHESES=' \
	'.endif'
# expect: Parse_PushInput: file variable-name-parentheses.tmp, line 1
# expect: Parse_PushInput: file variable-name-parentheses.tmp, line 1

# The guard condition must consist of only the guard variable, nothing else.
INCS+=	variable-ifndef-plus
LINES.variable-ifndef-plus= \
	'.ifndef VARIABLE_IFNDEF_PLUS && VARIABLE_IFNDEF_SECOND' \
	'VARIABLE_IFNDEF_PLUS=' \
	'VARIABLE_IFNDEF_SECOND=' \
	'.endif'
# expect: Parse_PushInput: file variable-ifndef-plus.tmp, line 1
# expect: Parse_PushInput: file variable-ifndef-plus.tmp, line 1

# The guard condition must consist of only the guard variable, nothing else.
INCS+=	variable-if-plus
LINES.variable-if-plus= \
	'.if !defined(VARIABLE_IF_PLUS) && !defined(VARIABLE_IF_SECOND)' \
	'VARIABLE_IF_PLUS=' \
	'VARIABLE_IF_SECOND=' \
	'.endif'
# expect: Parse_PushInput: file variable-if-plus.tmp, line 1
# expect: Parse_PushInput: file variable-if-plus.tmp, line 1

# The variable name in an '.ifndef' guard must be given directly, it must not
# contain any '$' expression.
INCS+=	variable-ifndef-indirect
LINES.variable-ifndef-indirect= \
	'.ifndef $${VARIABLE_IFNDEF_INDIRECT:L}' \
	'VARIABLE_IFNDEF_INDIRECT=' \
	'.endif'
# expect: Parse_PushInput: file variable-ifndef-indirect.tmp, line 1
# expect: Parse_PushInput: file variable-ifndef-indirect.tmp, line 1

# The variable name in an '.if' guard must be given directly, it must not
# contain any '$' expression.
INCS+=	variable-if-indirect
LINES.variable-if-indirect= \
	'.if !defined($${VARIABLE_IF_INDIRECT:L})' \
	'VARIABLE_IF_INDIRECT=' \
	'.endif'
# expect: Parse_PushInput: file variable-if-indirect.tmp, line 1
# expect: Parse_PushInput: file variable-if-indirect.tmp, line 1

# The variable name in the guard condition must only contain alphanumeric
# characters and underscores.  The place where the guard variable is defined
# is more flexible, as long as the variable is defined at the point where the
# file is included the next time.
INCS+=	variable-assign-indirect
LINES.variable-assign-indirect= \
	'.ifndef VARIABLE_ASSIGN_INDIRECT' \
	'$${VARIABLE_ASSIGN_INDIRECT:L}=' \
	'.endif'
# expect: Parse_PushInput: file variable-assign-indirect.tmp, line 1
# expect: Skipping 'variable-assign-indirect.tmp' because 'VARIABLE_ASSIGN_INDIRECT' is defined

# The time at which the guard variable is defined doesn't matter, as long as
# it is defined at the point where the file is included the next time.
INCS+=	variable-assign-late
LINES.variable-assign-late= \
	'.ifndef VARIABLE_ASSIGN_LATE' \
	'VARIABLE_ASSIGN_LATE_OTHER=' \
	'VARIABLE_ASSIGN_LATE=' \
	'.endif'
# expect: Parse_PushInput: file variable-assign-late.tmp, line 1
# expect: Skipping 'variable-assign-late.tmp' because 'VARIABLE_ASSIGN_LATE' is defined

# The time at which the guard variable is defined doesn't matter, as long as
# it is defined at the point where the file is included the next time.
INCS+=	variable-assign-nested
LINES.variable-assign-nested= \
	'.ifndef VARIABLE_ASSIGN_NESTED' \
	'.  if 1' \
	'.    for i in once' \
	'VARIABLE_ASSIGN_NESTED=' \
	'.    endfor' \
	'.  endif' \
	'.endif'
# expect: Parse_PushInput: file variable-assign-nested.tmp, line 1
# expect: Skipping 'variable-assign-nested.tmp' because 'VARIABLE_ASSIGN_NESTED' is defined

# If the guard variable is defined before the file is included for the first
# time, the file is considered guarded as well.  In such a case, the parser
# skips almost all lines, as they are irrelevant, but the structure of the
# top-level '.if/.endif' conditional can be determined reliably enough to
# decide whether the file is guarded.
INCS+=	variable-already-defined
LINES.variable-already-defined= \
	'.ifndef VARIABLE_ALREADY_DEFINED' \
	'VARIABLE_ALREADY_DEFINED=' \
	'.endif'
VARIABLE_ALREADY_DEFINED=
# expect: Parse_PushInput: file variable-already-defined.tmp, line 1
# expect: Skipping 'variable-already-defined.tmp' because 'VARIABLE_ALREADY_DEFINED' is defined

# If the guard variable is defined before the file is included the first time,
# the file is processed but its content is skipped.  If that same guard
# variable is undefined when the file is included the second time, the file is
# processed as usual.
INCS+=	variable-defined-then-undefined
LINES.variable-defined-then-undefined= \
	'.ifndef VARIABLE_DEFINED_THEN_UNDEFINED' \
	'.endif'
VARIABLE_DEFINED_THEN_UNDEFINED=
UNDEF_BETWEEN.variable-defined-then-undefined= \
	VARIABLE_DEFINED_THEN_UNDEFINED
# expect: Parse_PushInput: file variable-defined-then-undefined.tmp, line 1
# expect: Parse_PushInput: file variable-defined-then-undefined.tmp, line 1

# The whole file content must be guarded by a single '.if' conditional, not by
# several, as each of these conditionals would require its separate guard.
# This case is not expected to occur in practice, as the two parts would
# rather be split into separate files.
INCS+=	variable-two-times
LINES.variable-two-times= \
	'.ifndef VARIABLE_TWO_TIMES_1' \
	'VARIABLE_TWO_TIMES_1=' \
	'.endif' \
	'.ifndef VARIABLE_TWO_TIMES_2' \
	'VARIABLE_TWO_TIMES_2=' \
	'.endif'
# expect: Parse_PushInput: file variable-two-times.tmp, line 1
# expect: Parse_PushInput: file variable-two-times.tmp, line 1

# When multiple files use the same guard variable name, the optimization of
# skipping the file affects each of these files.
#
# Choosing unique guard names is the responsibility of the makefile authors.
# A typical pattern of guard variable names is '${PROJECT}_${DIR}_${FILE}_MK'.
# System-provided files typically start the guard names with '_'.
INCS+=	variable-clash
LINES.variable-clash= \
	${LINES.variable-if}
# expect: Parse_PushInput: file variable-clash.tmp, line 1
# expect: Skipping 'variable-clash.tmp' because 'VARIABLE_IF' is defined

# The conditional must come before the assignment, otherwise the conditional
# is useless, as it always evaluates to false.
INCS+=	variable-swapped
LINES.variable-swapped= \
	'SWAPPED=' \
	'.ifndef SWAPPED' \
	'.  error' \
	'.endif'
# expect: Parse_PushInput: file variable-swapped.tmp, line 1
# expect: Parse_PushInput: file variable-swapped.tmp, line 1

# If the guard variable is undefined between the first and the second time the
# file is included, the guarded file is included again.
INCS+=	variable-undef-between
LINES.variable-undef-between= \
	'.ifndef VARIABLE_UNDEF_BETWEEN' \
	'VARIABLE_UNDEF_BETWEEN=' \
	'.endif'
UNDEF_BETWEEN.variable-undef-between= \
	VARIABLE_UNDEF_BETWEEN
# expect: Parse_PushInput: file variable-undef-between.tmp, line 1
# expect: Parse_PushInput: file variable-undef-between.tmp, line 1

# If the guard variable is undefined while the file is included the first
# time, the guard does not have an effect, and the file is included again.
INCS+=	variable-undef-inside
LINES.variable-undef-inside= \
	'.ifndef VARIABLE_UNDEF_INSIDE' \
	'VARIABLE_UNDEF_INSIDE=' \
	'.undef VARIABLE_UNDEF_INSIDE' \
	'.endif'
# expect: Parse_PushInput: file variable-undef-inside.tmp, line 1
# expect: Parse_PushInput: file variable-undef-inside.tmp, line 1

# If the file does not define the guard variable, the guard does not have an
# effect, and the file is included again.
INCS+=	variable-not-defined
LINES.variable-not-defined= \
	'.ifndef VARIABLE_NOT_DEFINED' \
	'.endif'
# expect: Parse_PushInput: file variable-not-defined.tmp, line 1
# expect: Parse_PushInput: file variable-not-defined.tmp, line 1

# The outermost '.if' must not have an '.elif' branch.
INCS+=	elif
LINES.elif= \
	'.ifndef ELIF' \
	'ELIF=' \
	'.elif 1' \
	'.endif'
# expect: Parse_PushInput: file elif.tmp, line 1
# expect: Parse_PushInput: file elif.tmp, line 1

# When a file with an '.if/.elif/.endif' conditional at the top level is
# included, it is never optimized, as one of its branches is taken.
INCS+=	elif-reuse
LINES.elif-reuse= \
	'.ifndef ELIF' \
	'syntax error' \
	'.elif 1' \
	'.endif'
# expect: Parse_PushInput: file elif-reuse.tmp, line 1
# expect: Parse_PushInput: file elif-reuse.tmp, line 1

# The outermost '.if' must not have an '.else' branch.
INCS+=	else
LINES.else= \
	'.ifndef ELSE' \
	'ELSE=' \
	'.else' \
	'.endif'
# expect: Parse_PushInput: file else.tmp, line 1
# expect: Parse_PushInput: file else.tmp, line 1

# When a file with an '.if/.else/.endif' conditional at the top level is
# included, it is never optimized, as one of its branches is taken.
INCS+=	else-reuse
LINES.else-reuse= \
	'.ifndef ELSE' \
	'syntax error' \
	'.else' \
	'.endif'
# expect: Parse_PushInput: file else-reuse.tmp, line 1
# expect: Parse_PushInput: file else-reuse.tmp, line 1

# The inner '.if' directives may have an '.elif' or '.else', and it doesn't
# matter which of their branches are taken.
INCS+=	inner-if-elif-else
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
# expect: Parse_PushInput: file inner-if-elif-else.tmp, line 1
# expect: Skipping 'inner-if-elif-else.tmp' because 'INNER_IF_ELIF_ELSE' is defined

# The guard can also be a target instead of a variable.  Using a target as a
# guard has the benefit that a target cannot be undefined once it is defined.
# The target should be declared '.NOTMAIN'.  Since the target names are
# usually chosen according to a pattern that doesn't interfere with real
# target names, they don't need to be declared '.PHONY' as they don't generate
# filesystem operations.
INCS+=	target
LINES.target= \
	'.if !target(__target.tmp__)' \
	'__target.tmp__: .NOTMAIN' \
	'.endif'
# expect: Parse_PushInput: file target.tmp, line 1
# expect: Skipping 'target.tmp' because '__target.tmp__' is defined

# When used for system files, the target name may include '<' and '>', for
# symmetry with the '.include <sys.mk>' directive.  The characters '<' and '>'
# are ordinary characters.
INCS+=	target-sys
LINES.target-sys= \
	'.if !target(__<target-sys.tmp>__)' \
	'__<target-sys.tmp>__: .NOTMAIN' \
	'.endif'
# expect: Parse_PushInput: file target-sys.tmp, line 1
# expect: Skipping 'target-sys.tmp' because '__<target-sys.tmp>__' is defined

# The target name may include variable references.  These references are
# expanded as usual.  Due to the current implementation, the expressions are
# evaluated twice:  Once for checking whether the condition evaluates to true,
# and once for determining the guard name.  This double evaluation should not
# matter in practice, as guard expressions are expected to be simple,
# deterministic and without side effects.
INCS+=	target-indirect
LINES.target-indirect= \
	'.if !target($${target-indirect.tmp:L})' \
	'target-indirect.tmp: .NOTMAIN' \
	'.endif'
# expect: Parse_PushInput: file target-indirect.tmp, line 1
# expect: Skipping 'target-indirect.tmp' because 'target-indirect.tmp' is defined

# A common form of guard target is __${.PARSEFILE}__.  This form can only be
# used if all files using this form have unique basenames.  To get a robust
# pattern based on the same idea, use __${.PARSEDIR}/${.PARSEFILE}__ instead.
# This form does not work when the basename contains whitespace characters, as
# it is not possible to define a target with whitespace, not even by cheating.
INCS+=	target-indirect-PARSEFILE
LINES.target-indirect-PARSEFILE= \
	'.if !target(__$${.PARSEFILE}__)' \
	'__$${.PARSEFILE}__: .NOTMAIN' \
	'.endif'
# expect: Parse_PushInput: file target-indirect-PARSEFILE.tmp, line 1
# expect: Skipping 'target-indirect-PARSEFILE.tmp' because '__target-indirect-PARSEFILE.tmp__' is defined

# Two files with different basenames can both use the same syntactic pattern
# for the target guard name, as the expressions expand to different strings.
INCS+=	target-indirect-PARSEFILE2
LINES.target-indirect-PARSEFILE2= \
	'.if !target(__$${.PARSEFILE}__)' \
	'__$${.PARSEFILE}__: .NOTMAIN' \
	'.endif'
# expect: Parse_PushInput: file target-indirect-PARSEFILE2.tmp, line 1
# expect: Skipping 'target-indirect-PARSEFILE2.tmp' because '__target-indirect-PARSEFILE2.tmp__' is defined

# Using plain .PARSEFILE without .PARSEDIR leads to name clashes.  The include
# guard is the same as in the test case 'target-indirect-PARSEFILE', as the
# guard name only contains the basename but not the directory name.  So even
# without defining the guard variable, the file is considered guarded.
INCS+=	subdir/target-indirect-PARSEFILE
LINES.subdir/target-indirect-PARSEFILE= \
	'.if !target(__$${.PARSEFILE}__)' \
	'.endif'
# expect: Parse_PushInput: file subdir/target-indirect-PARSEFILE.tmp, line 1
# expect: Skipping 'subdir/target-indirect-PARSEFILE.tmp' because '__target-indirect-PARSEFILE.tmp__' is defined

# Another common form of guard target is __${.PARSEDIR}/${.PARSEFILE}__
# or __${.PARSEDIR:tA}/${.PARSEFILE}__ to be truly unique.
INCS+=	target-indirect-PARSEDIR-PARSEFILE
LINES.target-indirect-PARSEDIR-PARSEFILE= \
	'.if !target(__$${.PARSEDIR}/$${.PARSEFILE}__)' \
	'__$${.PARSEDIR}/$${.PARSEFILE}__: .NOTMAIN' \
	'.endif'
# expect: Parse_PushInput: file target-indirect-PARSEDIR-PARSEFILE.tmp, line 1
# expect: Skipping 'target-indirect-PARSEDIR-PARSEFILE.tmp' because '__target-indirect-PARSEDIR-PARSEFILE.tmp__' is defined
# The actual target starts with '__${.OBJDIR}/', see the .rawout file, but the
# string '${.OBJDIR}/' gets stripped in post processing.

# Using the combination of '.PARSEDIR' and '.PARSEFILE', a file in a
# subdirectory gets a different guard target name than the previous one.
INCS+=	subdir/target-indirect-PARSEDIR-PARSEFILE
LINES.subdir/target-indirect-PARSEDIR-PARSEFILE= \
	'.if !target(__$${.PARSEDIR}/$${.PARSEFILE}__)' \
	'__$${.PARSEDIR}/$${.PARSEFILE}__: .NOTMAIN' \
	'.endif'
# expect: Parse_PushInput: file subdir/target-indirect-PARSEDIR-PARSEFILE.tmp, line 1
# expect: Skipping 'subdir/target-indirect-PARSEDIR-PARSEFILE.tmp' because '__subdir/target-indirect-PARSEDIR-PARSEFILE.tmp__' is defined
# The actual target starts with '__${.OBJDIR}/', see the .rawout file, but the
# string '${.OBJDIR}/' gets stripped in post processing.

# If the guard target is not defined when including the file the next time,
# the file is processed again.
INCS+=	target-unguarded
LINES.target-unguarded= \
	'.if !target(target-unguarded)' \
	'.endif'
# expect: Parse_PushInput: file target-unguarded.tmp, line 1
# expect: Parse_PushInput: file target-unguarded.tmp, line 1

# The guard condition must consist of only the guard target, nothing else.
INCS+=	target-plus
LINES.target-plus= \
	'.if !target(target-plus) && 1' \
	'target-plus: .NOTMAIN' \
	'.endif'
# expect: Parse_PushInput: file target-plus.tmp, line 1
# expect: Parse_PushInput: file target-plus.tmp, line 1

# If the guard target is defined before the file is included the first time,
# the file is read once and then considered guarded.
INCS+=	target-already-defined
LINES.target-already-defined= \
	'.if !target(target-already-defined)' \
	'target-already-defined: .NOTMAIN' \
	'.endif'
target-already-defined: .NOTMAIN
# expect: Parse_PushInput: file target-already-defined.tmp, line 1
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
INCS+=	target-name-exclamation
LINES.target-name-exclamation= \
	'.if !target(!target-name-exclamation)' \
	'\!target-name-exclamation: .NOTMAIN' \
	'.endif'
# expect: Parse_PushInput: file target-name-exclamation.tmp, line 1
# expect: Parse_PushInput: file target-name-exclamation.tmp, line 1


# Now run all test cases by including each of the files twice and looking at
# the debug output.  The files that properly guard against multiple inclusion
# generate a 'Skipping' line, the others repeat the 'Parse_PushInput' line.
#
# Some debug output lines are suppressed in the .exp file, see ./Makefile.
.for i in ${INCS}
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
