# Copyright (C) 2012 by the Massachusetts Institute of Technology.
# All rights reserved.
#
# Export of this software from the United States of America may
#   require a specific license from the United States Government.
#   It is the responsibility of any person or organization contemplating
#   export to obtain such a license before exporting.
#
# WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
# distribute this software and its documentation for any purpose and
# without fee is hereby granted, provided that the above copyright
# notice appear in all copies and that both that copyright notice and
# this permission notice appear in supporting documentation, and that
# the name of M.I.T. not be used in advertising or publicity pertaining
# to distribution of the software without specific, written prior
# permission.  Furthermore if you modify this software you must label
# your software as modified software and not distribute it in such a
# fashion that it might be confused with the original M.I.T. software.
# M.I.T. makes no representations about the suitability of
# this software for any purpose.  It is provided "as is" without express
# or implied warranty.

# This program checks for some kinds of MIT krb5 coding style
# violations in a single file.  Checked violations include:
#
#   Line is too long
#   Tabs violations
#   Trailing whitespace and final blank lines
#   Comment formatting errors
#   Preprocessor statements in function bodies
#   Misplaced braces
#   Space before paren in function call, or no space after if/for/while
#   Parenthesized return expression
#   Space after cast operator, or no space before * in cast operator
#   Line broken before binary operator
#   Lack of spaces around binary operator (sometimes)
#   Assignment at the beginning of an if conditional
#   Use of prohibited string functions
#   Lack of braces around 2+ line flow control body
#   Incorrect indentation as determined by emacs c-mode (if possible)
#
# This program does not check for the following:
#
#   Anything outside of a function body except line length/whitespace
#   Anything non-syntactic (proper cleanup flow control, naming, etc.)
#   UTF-8 violations
#   Implicit tests against NULL or '\0'
#   Inner-scope variable declarations
#   Over- or under-parenthesization
#   Long or deeply nested function bodies
#   Syntax of function calls through pointers

import os
import re
import sys
from subprocess import call
from tempfile import NamedTemporaryFile

def warn(ln, msg):
    print('%5d  %s' % (ln, msg))


# If lines[0] indicates the krb5 C style, try to use emacs to reindent
# a copy of lines.  Return None if the file does not use the krb5 C
# style or if the emacs batch reindent is unsuccessful.
def emacs_reindent(lines):
    if 'c-basic-offset: 4; indent-tabs-mode: nil' not in lines[0]:
        return None

    util_dir = os.path.dirname(sys.argv[0])
    cstyle_el = os.path.join(util_dir, 'krb5-c-style.el')
    reindent_el = os.path.join(util_dir, 'krb5-batch-reindent.el')
    with NamedTemporaryFile(suffix='.c', mode='w+') as f:
        f.write(''.join(lines))
        f.flush()
        args = ['emacs', '-q', '-batch', '-l', cstyle_el, '-l', reindent_el,
                f.name]
        with open(os.devnull, 'w') as devnull:
            try:
                st = call(args, stdin=devnull, stdout=devnull, stderr=devnull)
                if st != 0:
                    return None
            except OSError:
                # Fail gracefully if emacs isn't installed.
                return None
        f.seek(0)
        ilines = f.readlines()
        f.close()
        return ilines


def check_length(line, ln):
    if len(line) > 79 and not line.startswith(' * Copyright'):
        warn(ln, 'Length exceeds 79 characters')


def check_tabs(line, ln, allow_tabs, seen_tab):
    if not allow_tabs:
        if '\t' in line:
            warn(ln, 'Tab character in file which does not allow tabs')
    else:
        if ' \t' in line:
            warn(ln, 'Tab character immediately following space')
        if '        ' in line and seen_tab:
            warn(ln, '8+ spaces in file which uses tabs')


def check_trailing_whitespace(line, ln):
    if line and line[-1] in ' \t':
        warn(ln, 'Trailing whitespace')


def check_comment(lines, ln):
    align = lines[0].index('/*') + 1
    if not lines[0].lstrip().startswith('/*'):
        warn(ln, 'Multi-line comment begins after code')
    for line in lines[1:]:
        ln += 1
        if len(line) <= align or line[align] != '*':
            warn(ln, 'Comment line does not have * aligned with top')
        elif line[:align].lstrip() != '':
            warn(ln, 'Garbage before * in comment line')
    if not lines[-1].rstrip().endswith('*/'):
        warn(ln, 'Code after end of multi-line comment')
    if len(lines) > 2 and (lines[0].strip() not in ('/*', '/**') or
                           lines[-1].strip() != '*/'):
        warn(ln, 'Comment is 3+ lines but is not formatted as block comment')


def check_preprocessor(line, ln):
    if line.startswith('#'):
        warn(ln, 'Preprocessor statement in function body')


def check_braces(line, ln):
    # Strip out one-line initializer expressions.
    line = re.sub(r'=\s*{.*}', '', line)
    if line.lstrip().startswith('{') and not line.startswith('{'):
        warn(ln, 'Un-cuddled open brace')
    if re.search(r'{\s*\S', line):
        warn(ln, 'Code on line after open brace')
    if re.search(r'\S.*}', line):
        warn(ln, 'Code on line before close brace')


# This test gives false positives on some function pointer type
# declarations or casts.  Avoid this by using typedefs.
def check_space_before_paren(line, ln):
    for m in re.finditer(r'([\w]+)(\s*)\(', line):
        ident, ws = m.groups()
        if ident in ('void', 'char', 'int', 'long', 'unsigned'):
            pass
        elif ident in ('if', 'for', 'while', 'switch'):
            if not ws:
                warn(ln, 'No space after flow control keyword')
        elif ident != 'return':
            if ws:
                warn(ln, 'Space before parenthesis in function call')

    if re.search(r' \)', line):
        warn(ln, 'Space before close parenthesis')


def check_parenthesized_return(line, ln):
    if re.search(r'return\s*\(.*\);', line):
        warn(ln, 'Parenthesized return expression')


def check_cast(line, ln):
    # We can't reliably distinguish cast operators from parenthesized
    # expressions or function call parameters without a real C parser,
    # so we use some heuristics.  A cast operator is followed by an
    # expression, which usually begins with an identifier or an open
    # paren.  A function call or parenthesized expression is never
    # followed by an identifier and only rarely by an open paren.  We
    # won't detect a cast operator when it's followed by an expression
    # beginning with '*', since it's hard to distinguish that from a
    # multiplication operator.  We will get false positives from
    # "(*fp) (args)" and "if (condition) statement", but both of those
    # are erroneous anyway.
    for m in re.finditer(r'\(([^(]+)\)(\s*)[a-zA-Z_(]', line):
        if m.group(2):
            warn(ln, 'Space after cast operator (or inline if/while body)')
        # Check for casts like (char*) which should have a space.
        if re.search(r'[^\s\*]\*+$', m.group(1)):
            warn(ln, 'No space before * in cast operator')


def check_binary_operator(line, ln):
    binop = r'(\+|-|\*|/|%|\^|==|=|!=|<=|<|>=|>|&&|&|\|\||\|)'
    if re.match(r'\s*' + binop + r'\s', line):
        warn(ln - 1, 'Line broken before binary operator')
    for m in re.finditer(r'(\s|\w)' + binop + r'(\s|\w)', line):
        before, op, after = m.groups()
        if not before.isspace() and not after.isspace():
            warn(ln, 'No space before or after binary operator')
        elif not before.isspace():
            warn(ln, 'No space before binary operator')
        elif op not in ('-', '*', '&') and not after.isspace():
            warn(ln, 'No space after binary operator')


def check_assignment_in_conditional(line, ln):
    # Check specifically for if statements; we allow assignments in
    # loop expressions.
    if re.search(r'if\s*\(+\w+\s*=[^=]', line):
        warn(ln, 'Assignment in if conditional')


def indent(line):
    return len(re.match('\s*', line).group(0).expandtabs())


def check_unbraced_flow_body(line, ln, lines):
    if re.match(r'\s*do$', line):
        warn(ln, 'do statement without braces')
        return

    m = re.match(r'\s*(})?\s*else(\s*if\s*\(.*\))?\s*({)?\s*$', line)
    if m and (m.group(1) is None) != (m.group(3) is None):
        warn(ln, 'One arm of if/else statement braced but not the other')

    if (re.match('\s*(if|else if|for|while)\s*\(.*\)$', line) or
        re.match('\s*else$', line)):
        base = indent(line)
        # Look at the next two lines (ln is 1-based so lines[ln] is next).
        if indent(lines[ln]) > base and indent(lines[ln + 1]) > base:
            warn(ln, 'Body is 2+ lines but has no braces')


def check_bad_string_fn(line, ln):
    # This is intentionally pretty fuzzy so that we catch the whole scanf
    if re.search(r'\W(strcpy|strcat|sprintf|\w*scanf)\W', line):
        warn(ln, 'Prohibited string function')


def check_indentation(line, indented_lines, ln):
    if not indented_lines:
        return

    if ln - 1 >= len(indented_lines):
        # This should only happen when the emacs reindent removed
        # blank lines from the input file, but check.
        if line.strip() == '':
            warn(ln, 'Trailing blank line')
        return

    if line != indented_lines[ln - 1].rstrip('\r\n'):
        warn(ln, 'Indentation may be incorrect')


def check_file(lines):
    # Check if this file allows tabs.
    if len(lines) == 0:
        return
    allow_tabs = 'indent-tabs-mode: nil' not in lines[0]
    seen_tab = False
    indented_lines = emacs_reindent(lines)

    in_function = False
    comment = []
    ln = 0
    for line in lines:
        ln += 1
        line = line.rstrip('\r\n')
        seen_tab = seen_tab or ('\t' in line)

        # Check line structure issues before altering the line.
        check_indentation(line, indented_lines, ln)
        check_length(line, ln)
        check_tabs(line, ln, allow_tabs, seen_tab)
        check_trailing_whitespace(line, ln)

        # Strip out single-line comments the contents of string literals.
        if not comment:
            line = re.sub(r'/\*.*?\*/', '', line)
            line = re.sub(r'"(\\.|[^"])*"', '""', line)

        # Parse out and check multi-line comments.  (Ignore code on
        # the first or last line; check_comment will warn about it.)
        if comment or '/*' in line:
            comment.append(line)
            if '*/' in line:
                check_comment(comment, ln - len(comment) + 1)
                comment = []
            continue

        # Warn if we see a // comment and ignore anything following.
        if '//' in line:
            warn(ln, '// comment')
            line = re.sub(r'//.*/', '', line)

        if line.startswith('{'):
            in_function = True
        elif line.startswith('}'):
            in_function = False

        if in_function:
            check_preprocessor(line, ln)
            check_braces(line, ln)
            check_space_before_paren(line, ln)
            check_parenthesized_return(line, ln)
            check_cast(line, ln)
            check_binary_operator(line, ln)
            check_assignment_in_conditional(line, ln)
            check_unbraced_flow_body(line, ln, lines)
            check_bad_string_fn(line, ln)

    if lines[-1] == '':
        warn(ln, 'Blank line at end of file')


if len(sys.argv) == 1:
    lines = sys.stdin.readlines()
elif len(sys.argv) == 2:
    f = open(sys.argv[1])
    lines = f.readlines()
    f.close()
else:
    sys.stderr.write('Usage: cstyle-file [filename]\n')
    sys.exit(1)

check_file(lines)
