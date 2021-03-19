# kernel message checker module
#
# Copyright (c) 2013, Intel Corporation
#
# Author: Johannes Berg <johannes@sipsolutions.net>
#
# This software may be distributed under the terms of the BSD license.
# See README for more details.
#
"""
Tests for kernel messages to find if there were any issues in them.
"""

import re

lockdep_messages = [
  'possible circular locking dependency',
  '.*-safe -> .*unsafe lock order detected',
  'possible recursive locking detected',
  'inconsistent lock state',
  'possible irq lock inversion dependency',
  'suspicious RCU usage',
]
lockdep = r'(\[\s*)?(INFO|WARNING): (%s)|\*\*\* DEADLOCK \*\*\*' % ('|'.join(lockdep_messages), )
issue = re.compile('(\[[0-9 .]*\] )?(WARNING:|BUG:|%s|RTNL: assertion failed).*' % lockdep)

def check_kernel(logfile):
    for line in open(logfile, 'r'):
        if issue.match(line):
            return False
    return True
