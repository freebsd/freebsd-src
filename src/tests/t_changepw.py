#!/usr/bin/python
from k5test import *

# This file is intended to cover any password-changing mechanism.  For
# now it only contains a regression test for #7868.

realm = K5Realm(create_host=False, get_creds=False, start_kadmind=True)

# Mark a principal as expired and change its password through kinit.
realm.run([kadminl, 'modprinc', '-pwexpire', '1 day ago', 'user'])
pwinput = password('user') + '\nabcd\nabcd\n'
realm.run([kinit, realm.user_princ], input=pwinput)

# Do the same thing with FAST, with tracing turned on.
realm.run([kadminl, 'modprinc', '-pwexpire', '1 day ago', 'user'])
pwinput = 'abcd\nefgh\nefgh\n'
tracefile = os.path.join(realm.testdir, 'trace')
realm.run(['env', 'KRB5_TRACE=' + tracefile, kinit, '-T', realm.ccache,
           realm.user_princ], input=pwinput)

# Read the trace and check that FAST was used when getting the
# kadmin/changepw ticket.
f = open(tracefile, 'r')
trace = f.read()
f.close()
getting_changepw = fast_used_for_changepw = False
for line in trace.splitlines():
    if 'Getting initial credentials for user@' in line:
        getting_changepw_ticket = False
    if 'Setting initial creds service to kadmin/changepw' in line:
        getting_changepw_ticket = True
    if getting_changepw_ticket and 'Using FAST' in line:
        fast_used_for_changepw = True
if not fast_used_for_changepw:
    fail('FAST was not used to get kadmin/changepw ticket')

success('Password change tests')
