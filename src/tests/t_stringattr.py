#!/usr/bin/python

# Copyright (C) 2011 by the Massachusetts Institute of Technology.
# All rights reserved.

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

from k5test import *

realm = K5Realm(start_kadmind=True, create_host=False, get_creds=False)

realm.prep_kadmin()

out = realm.run_kadmin(['getstrs', 'user'])
if '(No string attributes.)' not in out:
    fail('Empty attribute query')

realm.run_kadmin(['setstr', 'user', 'attr1', 'value1'])
realm.run_kadmin(['setstr', 'user', 'attr2', 'value2'])
realm.run_kadmin(['delstr', 'user', 'attr1'])
realm.run_kadmin(['setstr', 'user', 'attr3', 'value3'])

out = realm.run_kadmin(['getstrs', 'user'])
if ('attr2: value2' not in out or 'attr3: value3' not in out or
    'attr1:' in out):
    fail('Final attribute query')

success('KDB string attributes')
