# Copyright (C) 2011 by the Massachusetts Institute of Technology.
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

# This program is intended to be used by "make check-copyright".  It
# checks for violations of the coding standards related to copyright
# and license statements in source code comments.

import os
import sys
import re

def warn(fname, ln, msg):
    print '%s: %d: %s' % (fname, ln + 1, msg)

def indicates_license(line):
    return 'Copyright' in line or 'COPYRIGHT' in line or 'License' in line

# Check a comment for boilerplate violations.  Return true if the comment
# is a license statement.
def check_comment(comment, fname, ln, code_seen, nonlicense_seen):
    text_seen = False
    is_license = False
    for line in comment:
        if not is_license and indicates_license(line):
            is_license = True
            if text_seen:
                warn(fname, ln, 'License begins after first line of comment')
            elif code_seen:
                warn(fname, ln, 'License after code')
            elif nonlicense_seen:
                warn(fname, ln, 'License after non-license comments')
            break
        # DB2 licenses start with '/*-' and we don't want to change them.
        if line != '' and line != '-':
            text_seen = True
    return is_license

def check_file(lines, fname):
    # Skip emacs mode line if present.
    ln = 0
    if '-*- mode: c;' in lines[ln]:
        ln += 1

    # Check filename comment if present.
    m = re.match(r'/\* ([^ ]*)( - .*)? \*/', lines[ln])
    if m:
        if m.group(1) != fname:
            warn(fname, ln, 'Wrong filename in comment')
        ln += 1

    # Scan for license statements.
    in_comment = False
    code_seen = False
    nonlicense_seen = False
    for line in lines[ln:]:
        # Strip out whitespace and comments contained within a line.
        if not in_comment:
            line = re.sub(r'/\*.*?\*/', '', line)
        line = line.strip()

        if not in_comment and '/*' in line:
            (line, sep, comment_part) = line.partition('/*')
            comment = [comment_part.strip()]
            comment_starts_at = ln
            in_comment = True
        elif in_comment and '*/' not in line:
            comment.append(line.lstrip('*').lstrip())
        elif in_comment:
            (comment_part, sep, line) = line.partition('*/')
            comment.append(comment_part.strip())
            is_license = check_comment(comment, fname, comment_starts_at,
                                       code_seen, nonlicense_seen)
            nonlicense_seen = nonlicense_seen or not is_license
            in_comment = False
        elif line.strip() != '':
            code_seen = True

        ln += 1

for fname in sys.argv[1:]:
    if fname.startswith('./'):
        fname = fname[2:]
    f = open(fname)
    lines = f.readlines()
    f.close()
    check_file(lines, fname)
