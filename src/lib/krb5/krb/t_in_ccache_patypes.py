#!/usr/bin/python

# Copyright (C) 2010,2012 by the Massachusetts Institute of Technology.
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
realm.run([kadminl, 'addprinc', '-pw', 'pass', 'nopreauth'])
realm.run([kadminl, 'addprinc', '-pw', 'pass', '+requires_preauth', 'preauth'])

# Check that we can get creds without preauth without an in_ccache.  This is
# the default behavior for kinit.
realm.run(['./t_in_ccache', 'nopreauth', 'pass'])

# Check that we can get creds with preauth without an in_ccache.  This is the
# default behavior for kinit.
realm.run(['./t_in_ccache', 'preauth', 'pass'])

# Check that we can get creds while supplying a now-populated input ccache that
# doesn't contain any relevant configuration.
realm.run(['./t_in_ccache', 'nopreauth', 'pass'])
realm.run(['./t_in_ccache', '-I', realm.ccache, 'preauth', 'pass'])

# Check that we can get creds while supplying a now-populated input ccache.
realm.run(['./t_in_ccache', 'preauth', 'pass'])
realm.run(['./t_in_ccache', '-I', realm.ccache, 'preauth', 'pass'])

# Check that we can't get creds while specifying patypes that aren't available
# in a FAST tunnel while using a FAST tunnel.  Expect the client-end
# preauth-failed error.
realm.run(['./t_in_ccache', 'nopreauth', 'pass'])
realm.run(['./t_cc_config', '-p', realm.krbtgt_princ, 'pa_type', '2'])
realm.run(['./t_in_ccache', '-A', realm.ccache, '-I', realm.ccache,
           'preauth', 'pass'], expected_code=210)

# Check that we can't get creds while specifying patypes that are only
# available in a FAST tunnel while not using a FAST tunnel.  Expect the
# client-end preauth-failed error.
realm.run(['./t_in_ccache', 'nopreauth', 'pass'])
realm.run(['./t_cc_config', '-p', realm.krbtgt_princ, 'pa_type', '138'])
realm.run(['./t_in_ccache', '-I', realm.ccache, 'preauth', 'pass'],
          expected_code=210)

# Check that we can get creds using FAST, and that we end up using
# encrypted_challenge when we do.
realm.run(['./t_in_ccache', 'preauth', 'pass'])
realm.run(['./t_cc_config', '-p', realm.krbtgt_princ, 'pa_type', '138'])
realm.run(['./t_in_ccache', '-A', realm.ccache, 'preauth', 'pass'])
output = realm.run(['./t_cc_config', '-p', realm.krbtgt_princ, 'pa_type'])
# We should have selected and used encrypted_challenge.
if output != '138':
    fail('Unexpected pa_type value in out_ccache: "%s"' % output)

# Check that we can get creds while specifying the right patypes.
realm.run(['./t_in_ccache', 'nopreauth', 'pass'])
realm.run(['./t_cc_config', '-p', realm.krbtgt_princ, 'pa_type', '2'])
realm.run(['./t_in_ccache', '-I', realm.ccache, 'preauth', 'pass'])
output = realm.run(['./t_cc_config', '-p', realm.krbtgt_princ, 'pa_type'])
# We should have selected and used encrypted_timestamp.
if output != '2':
    fail('Unexpected pa_type value in out_ccache')

success('input ccache pa_type tests')
