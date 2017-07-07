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

enctype = "aes128-cts"

realm = K5Realm(create_host=False, create_user=False)
salttypes = ('normal', 'v4', 'norealm', 'onlyrealm')

# For a variety of salt types, test that we can rename a principal and
# still get tickets with the same password.
for st in salttypes:
    realm.run([kadminl, 'addprinc', '-e', enctype + ':' + st,
               '-pw', password(st), st])
    realm.kinit(st, password(st))
    newprinc = 'new' + st
    realm.run([kadminl, 'renprinc', st, newprinc])
    realm.kinit(newprinc, password(st))

# Rename the normal salt again to test renaming a principal with
# special salt type (which it will have after the first rename).
realm.run([kadminl, 'renprinc', 'newnormal', 'newnormal2'])
realm.kinit('newnormal2', password('normal'))

success('Principal renaming tests')
