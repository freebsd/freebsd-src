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

# This program attempts to detect MIT krb5 coding style violations
# attributable to the changes a series of git commits.  It can be run
# from anywhere within a git working tree.

import getopt
import os
import re
import sys
from subprocess import Popen, PIPE, call

def usage():
    u = ['Usage: cstyle [-w] [rev|rev1..rev2]',
         '',
         'By default, checks working tree against HEAD, or checks changes in',
         'HEAD if the working tree is clean.  With a revision option, checks',
         'changes in rev or the series rev1..rev2.  With the -w option,',
         'checks working tree against rev (defaults to HEAD).']
    sys.stderr.write('\n'.join(u) + '\n')
    sys.exit(1)


# Run a command and return a list of its output lines.
def run(args):
    # subprocess.check_output would be ideal here, but requires Python 2.7.
    p = Popen(args, stdout=PIPE, stderr=PIPE)
    out, err = p.communicate()
    if p.returncode != 0:
        sys.stderr.write('Failed command: ' + ' '.join(args) + '\n')
        if err != '':
            sys.stderr.write('stderr:\n' + err)
        sys.stderr.write('Unexpected command failure, exiting\n')
        sys.exit(1)
    return out.splitlines()


# Find the top level of the git working tree, or None if we're not in
# one.
def find_toplevel():
    # git doesn't seem to have a way to do this, so we search by hand.
    dir = os.getcwd()
    while True:
        if os.path.exists(os.path.join(dir, '.git')):
            break
        parent = os.path.dirname(dir)
        if (parent == dir):
            return None
        dir = parent
    return dir


# Check for style issues in a file within rev (or within the current
# checkout if rev is None).  Report only problems on line numbers in
# new_lines.
line_re = re.compile(r'^\s*(\d+)  (.*)$')
def check_file(filename, rev, new_lines):
    # Process only C source files under src.
    root, ext = os.path.splitext(filename)
    if not filename.startswith('src/') or ext not in ('.c', '.h', '.hin'):
        return
    dispname = filename[4:]

    if rev is None:
        p1 = Popen(['cat', filename], stdout=PIPE)
    else:
        p1 = Popen(['git', 'show', rev + ':' + filename], stdout=PIPE)
    p2 = Popen(['python', 'src/util/cstyle-file.py'], stdin=p1.stdout,
               stdout=PIPE)
    p1.stdout.close()
    out, err = p2.communicate()
    if p2.returncode != 0:
        sys.exit(1)

    first = True
    for line in out.splitlines():
        m = line_re.match(line)
        if int(m.group(1)) in new_lines:
            if first:
                print '  ' + dispname + ':'
                first = False
            print '    ' + line


# Determine the lines of each file modified by diff (a sequence of
# strings) and check for style violations in those lines.  rev
# indicates the version in which the new contents of each file can be
# found, or is None if the current contents are in the working copy.
chunk_header_re = re.compile(r'^@@ -\d+(,(\d+))? \+(\d+)(,(\d+))? @@')
def check_diff(diff, rev):
    old_count, new_count, lineno = 0, 0, 0
    filename = None
    for line in diff:
        if not line or line.startswith('\\ No newline'):
            continue
        if old_count > 0 or new_count > 0:
            # We're in a chunk.
            if line[0] == '+':
                new_lines.append(lineno)
            if line[0] in ('+', ' '):
                new_count = new_count - 1
                lineno = lineno + 1
            if line[0] in ('-', ' '):
                old_count = old_count - 1
        elif line.startswith('+++ b/'):
            # We're starting a new file.  Check the last one.
            if filename:
                check_file(filename, rev, new_lines)
            filename = line[6:]
            new_lines = []
        else:
            m = chunk_header_re.match(line)
            if m:
                old_count = int(m.group(2) or '1')
                lineno = int(m.group(3))
                new_count = int(m.group(5) or '1')

    # Check the last file in the diff.
    if filename:
        check_file(filename, rev, new_lines)


# Check a sequence of revisions for style issues.
def check_series(revlist):
    for rev in revlist:
        sys.stdout.flush()
        call(['git', 'show', '-s', '--oneline', rev])
        diff = run(['git', 'diff-tree', '--no-commit-id', '--root', '-M',
                    '--cc', rev])
        check_diff(diff, rev)


# Parse arguments.
try:
    opts, args = getopt.getopt(sys.argv[1:], 'w')
except getopt.GetoptError, err:
    print str(err)
    usage()
if len(args) > 1:
    usage()

# Change to the top level of the working tree so we easily run the file
# checker and refer to working tree files.
toplevel = find_toplevel()
if toplevel is None:
    sys.stderr.write('%s must be run within a git working tree')
os.chdir(toplevel)

if ('-w', '') in opts:
    # Check the working tree against a base revision.
    arg = 'HEAD'
    if args:
        arg = args[0]
    check_diff(run(['git', 'diff', arg]), None)
elif args:
    # Check the differences in a rev or a series of revs.
    if '..' in args[0]:
        check_series(run(['git', 'rev-list', '--reverse', args[0]]))
    else:
        check_series([args[0]])
else:
    # No options or arguments.  Check the differences against HEAD, or
    # the differences in HEAD if the working tree is clean.
    diff = run(['git', 'diff', 'HEAD'])
    if diff:
        check_diff(diff, None)
    else:
        check_series(['HEAD'])
