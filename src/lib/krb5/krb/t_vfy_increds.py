#!/usr/bin/python

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

from k5test import *

realm = K5Realm()

# Verify the default test realm credentials with the default keytab.
realm.run(['./t_vfy_increds'])
realm.run(['./t_vfy_increds', '-n'])

# Verify after updating the keytab (so the keytab contains an outdated
# version 1 key followed by an up-to-date version 2 key).
realm.run([kadminl, 'ktadd', realm.host_princ])
realm.run(['./t_vfy_increds'])
realm.run(['./t_vfy_increds', '-n'])

# Bump the host key without updating the keytab and make sure that
# verification fails as we expect it to.
realm.run([kadminl, 'change_password', '-randkey', realm.host_princ])
realm.run(['./t_vfy_increds'], expected_code=1)
realm.run(['./t_vfy_increds', '-n'], expected_code=1)

# Simulate a system where the hostname has changed and the keytab
# contains host service principals with a hostname that no longer
# matches.  Verify after updating the keytab with a host service
# principal that has hostname that doesn't match the host running the
# test.  Verify should succeed, with or without nofail.
realm.run([kadminl, 'addprinc', '-randkey', 'host/wrong.hostname'])
realm.run([kadminl, 'ktadd', 'host/wrong.hostname'])
realm.run(['./t_vfy_increds'])
realm.run(['./t_vfy_increds', '-n'])

# Remove the keytab and verify again.  This should succeed if nofail
# is not set, and fail if it is set.
os.remove(realm.keytab)
realm.run(['./t_vfy_increds'])
realm.run(['./t_vfy_increds', '-n'], expected_code=1)

# Create an empty keytab file and verify again.  This simulates a
# system where an admin ran "touch krb5.keytab" to work around a
# Solaris Kerberos bug where krb5_kt_default() fails if the keytab
# file doesn't exist.  Verification should succeed in nofail is not
# set.  (An empty keytab file appears as corrupt to keytab calls,
# causing a KRB5_KEYTAB_BADVNO error, so any tightening of the
# krb5_verify_init_creds semantics needs to take this into account.)
open(realm.keytab, 'w').close()
realm.run(['./t_vfy_increds'])
realm.run(['./t_vfy_increds', '-n'], expected_code=1)
os.remove(realm.keytab)

# Add an NFS service principal to keytab.  Verify should ignore it by
# default (succeeding unless nofail is set), but should verify with it
# when it is specifically requested.
realm.run([kadminl, 'addprinc', '-randkey', realm.nfs_princ])
realm.run([kadminl, 'ktadd', realm.nfs_princ])
realm.run(['./t_vfy_increds'])
realm.run(['./t_vfy_increds', '-n'], expected_code=1)
realm.run(['./t_vfy_increds', realm.nfs_princ])
realm.run(['./t_vfy_increds', '-n', realm.nfs_princ])

# Invalidating the NFS keys in the keytab.  We should get the same
# results with the default principal argument, but verification should
# now fail if we request it specifically.
realm.run([kadminl, 'change_password', '-randkey', realm.nfs_princ])
realm.run(['./t_vfy_increds'])
realm.run(['./t_vfy_increds', '-n'], expected_code=1)
realm.run(['./t_vfy_increds', realm.nfs_princ], expected_code=1)
realm.run(['./t_vfy_increds', '-n', realm.nfs_princ], expected_code=1)

# Spot-check that verify_ap_req_nofail works equivalently to the
# programmatic nofail option.
realm.stop()
conf = {'libdefaults': {'verify_ap_req_nofail': 'true'}}
realm = K5Realm(krb5_conf=conf)
os.remove(realm.keytab)
realm.run(['./t_vfy_increds'], expected_code=1)

success('krb5_verify_init_creds tests')
