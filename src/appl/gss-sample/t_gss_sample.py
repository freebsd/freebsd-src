#!/usr/bin/python

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

appdir = os.path.join(buildtop, 'appl', 'gss-sample')
gss_client = os.path.join(appdir, 'gss-client')
gss_server = os.path.join(appdir, 'gss-server')

# Run a gss-server process and a gss-client process, with additional
# gss-client flags given by options and additional gss-server flags
# given by server_options.  Return the output of gss-client.
def run_client_server(realm, options, server_options, expected_code=0):
    portstr = str(realm.server_port())
    server_args = [gss_server, '-export', '-port', portstr]
    server_args += server_options + ['host']
    server = realm.start_server(server_args, 'starting...')
    out = realm.run([gss_client, '-port', portstr] + options +
                    [hostname, 'host', 'testmsg'], expected_code=expected_code)
    stop_daemon(server)
    return out

# Run a gss-server and gss-client process, and verify that gss-client
# displayed the expected output for a successful negotiation.
def server_client_test(realm, options, server_options):
    out = run_client_server(realm, options, server_options)
    if 'Signature verified.' not in out:
        fail('Expected message not seen in gss-client output')

# Make up a filename to hold user's initial credentials.
def ccache_savefile(realm):
    return os.path.join(realm.testdir, 'ccache.copy')

# Move user's initial credentials into the save file.
def ccache_save(realm):
    os.rename(realm.ccache, ccache_savefile(realm))

# Copy user's initial credentials from the save file into the ccache.
def ccache_restore(realm):
    shutil.copyfile(ccache_savefile(realm), realm.ccache)

# Perform a regular (TGS path) test of the server and client.
def tgs_test(realm, options, server_options=[]):
    ccache_restore(realm)
    server_client_test(realm, options, server_options)
    realm.klist(realm.user_princ, realm.host_princ)

# Perform a test of the server and client with initial credentials
# obtained through gss_acquire_cred_with_password().
def pw_test(realm, options, server_options=[]):
    if os.path.exists(realm.ccache):
        os.remove(realm.ccache)
    options = options + ['-user', realm.user_princ, '-pass', password('user')]
    server_client_test(realm, options, server_options)
    if os.path.exists(realm.ccache):
        fail('gss_acquire_cred_with_password created ccache')

# Perform a test using the wrong password, and make sure that failure
# occurs during the expected operation (gss_init_sec_context() for
# IAKERB, gss_aqcuire_cred_with_password() otherwise).
def wrong_pw_test(realm, options, server_options=[], iakerb=False):
    options = options + ['-user', realm.user_princ, '-pass', 'wrongpw']
    out = run_client_server(realm, options, server_options, expected_code=1)
    failed_op = 'initializing context' if iakerb else 'acquiring creds'
    if 'GSS-API error ' + failed_op not in out:
        fail('Expected error not seen in gss-client output')

# Perform a test of the server and client with initial credentials
# obtained with the client keytab
def kt_test(realm, options, server_options=[]):
    if os.path.exists(realm.ccache):
        os.remove(realm.ccache)
    server_client_test(realm, options, server_options)
    realm.klist(realm.user_princ, realm.host_princ)

for realm in multipass_realms():
    ccache_save(realm)

    tgs_test(realm, ['-krb5'])
    tgs_test(realm, ['-spnego'])
    tgs_test(realm, ['-iakerb'], ['-iakerb'])
    # test default (i.e., krb5) mechanism with GSS_C_DCE_STYLE
    tgs_test(realm, ['-dce'])

    pw_test(realm, ['-krb5'])
    pw_test(realm, ['-spnego'])
    pw_test(realm, ['-iakerb'], ['-iakerb'])
    pw_test(realm, ['-dce'])

    wrong_pw_test(realm, ['-krb5'])
    wrong_pw_test(realm, ['-spnego'])
    wrong_pw_test(realm, ['-iakerb'], ['-iakerb'], True)
    wrong_pw_test(realm, ['-dce'])

    realm.extract_keytab(realm.user_princ, realm.client_keytab)
    kt_test(realm, ['-krb5'])
    kt_test(realm, ['-spnego'])
    kt_test(realm, ['-iakerb'], ['-iakerb'])
    kt_test(realm, ['-dce'])

success('GSS sample application')
