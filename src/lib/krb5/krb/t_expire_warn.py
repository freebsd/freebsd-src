# Copyright (C) 2010 by the Massachusetts Institute of Technology.
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

from k5test import *

# Create a bare-bones KDC.
realm = K5Realm(create_user=False, create_host=False)

# Create principals with various password expirations.
realm.run([kadminl, 'addprinc', '-pw', 'pass', 'noexpire'])
realm.run([kadminl, 'addprinc', '-pw', 'pass', '-pwexpire', '30 minutes',
           'minutes'])
realm.run([kadminl, 'addprinc', '-pw', 'pass', '-pwexpire', '12 hours',
           'hours'])
realm.run([kadminl, 'addprinc', '-pw', 'pass', '-pwexpire', '3 days', 'days'])

# Check for expected prompter warnings when no expire callback is used.
output = realm.run(['./t_expire_warn', 'noexpire', 'pass', '0', '0'])
if output:
    fail('Unexpected output for noexpire')
realm.run(['./t_expire_warn', 'minutes', 'pass', '0', '0'],
          expected_msg=' less than one hour on ')
realm.run(['./t_expire_warn', 'hours', 'pass', '0', '0'],
          expected_msg=' hours on ')
realm.run(['./t_expire_warn', 'days', 'pass', '0', '0'],
          expected_msg=' days on ')
# Try one case with the stepwise interface.
realm.run(['./t_expire_warn', 'days', 'pass', '0', '1'],
          expected_msg=' days on ')

# Check for expected expire callback behavior.  These tests are
# carefully agnostic about whether the KDC supports last_req fields,
# and could be made more specific if last_req support is added.
output = realm.run(['./t_expire_warn', 'noexpire', 'pass', '1', '0'])
if 'password_expiration = 0\n' not in output or \
        'account_expiration = 0\n' not in output or \
        'is_last_req = ' not in output:
    fail('Expected callback output not seen for noexpire')
output = realm.run(['./t_expire_warn', 'days', 'pass', '1', '0'])
if 'password_expiration = ' not in output or \
        'password_expiration = 0\n' in output:
    fail('Expected non-zero password expiration not seen for days')
# Try one case with the stepwise interface.
output = realm.run(['./t_expire_warn', 'days', 'pass', '1', '1'])
if 'password_expiration = ' not in output or \
        'password_expiration = 0\n' in output:
    fail('Expected non-zero password expiration not seen for days')

success('Password expiration warning tests')
