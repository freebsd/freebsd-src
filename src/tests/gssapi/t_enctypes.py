#!/usr/bin/python
from k5test import *

# Define some convenience abbreviations for enctypes we will see in
# test program output.  For background, aes256 and aes128 are "CFX
# enctypes", meaning that they imply support for RFC 4121, while des3
# and rc4 are not.  DES3 keys will appear as 'des3-cbc-raw' in
# t_enctypes output because that's how GSSAPI does raw triple-DES
# encryption without the RFC3961 framing.
aes256 = 'aes256-cts-hmac-sha1-96'
aes128 = 'aes128-cts-hmac-sha1-96'
des3 = 'des3-cbc-sha1'
des3raw = 'des3-cbc-raw'
rc4 = 'arcfour-hmac'

# These tests make assumptions about the default enctype lists, so set
# them explicitly rather than relying on the library defaults.
enctypes='aes des3 rc4'
supp='aes256-cts:normal aes128-cts:normal des3-cbc-sha1:normal rc4-hmac:normal'
conf = {'libdefaults': {
        'default_tgs_enctypes': enctypes,
        'default_tkt_enctypes': enctypes,
        'permitted_enctypes': enctypes},
        'realms': {'$realm': {'supported_enctypes': supp}}}
realm = K5Realm(krb5_conf=conf)
shutil.copyfile(realm.ccache, os.path.join(realm.testdir, 'save'))

# Return an argument list for running t_enctypes with optional initiator
# and acceptor enctype lists.
def cmdline(ienc, aenc):
    iflags = ienc and ['-i', ienc] or []
    aflags = aenc and ['-a', aenc] or []
    return ['./t_enctypes'] + iflags + aflags + ['p:' + realm.host_princ]


# Run t_enctypes with optional initiator and acceptor enctype lists,
# and check that it succeeds with the expected output.  Also check
# that the ticket we got has the expected encryption key and session
# key.
def test(msg, ienc, aenc, tktenc='', tktsession='', proto='', isubkey='',
         asubkey=None):
    shutil.copyfile(os.path.join(realm.testdir, 'save'), realm.ccache)
    # Run the test program and check its output.
    out = realm.run(cmdline(ienc, aenc)).split()
    if out[0] != proto or out[1] != isubkey:
        fail(msg)
    if asubkey is not None and (len(out) < 3 or out[2] != asubkey):
        fail(msg)
    lines = realm.run([klist, '-e']).splitlines()
    for ind, line in enumerate(lines):
        if realm.host_princ in line:
            if lines[ind + 1].strip() != ('Etype (skey, tkt): %s, %s' %
                                          (tktsession, tktenc)):
                fail(msg)
            break

# Run t_enctypes with optional initiator and acceptor enctype lists,
# and check that it fails with the expected error message.
def test_err(msg, ienc, aenc, expected_err):
    shutil.copyfile(os.path.join(realm.testdir, 'save'), realm.ccache)
    out = realm.run(cmdline(ienc, aenc), expected_code=1)
    if expected_err not in out:
        fail(msg)


# By default, all of the key enctypes should be aes256.
test('noargs', None, None,
     tktenc=aes256, tktsession=aes256,
     proto='cfx', isubkey=aes256, asubkey=aes256)

# When the initiator constrains the permitted session enctypes to
# aes128, the ticket encryption key should remain aes256.  The client
# initiator will not send an RFC 4537 upgrade list because it sees no
# other permitted enctypes, so the acceptor subkey will not be
# upgraded from aes128.
test('init aes128', 'aes128-cts', None,
     tktenc=aes256, tktsession=aes128,
     proto='cfx', isubkey=aes128, asubkey=aes128)

# If the initiator and acceptor both constrain the permitted session
# enctypes to aes128, we should see the same keys as above.  This
# tests that the acceptor does not mistakenly contrain the ticket
# encryption key.
test('both aes128', 'aes128-cts', 'aes128-cts',
     tktenc=aes256, tktsession=aes128,
     proto='cfx', isubkey=aes128, asubkey=aes128)

# If only the acceptor constrains the permitted session enctypes to
# aes128, subkey negotiation fails because the acceptor considers the
# aes256 session key to be non-permitted.
test_err('acc aes128', None, 'aes128-cts', 'Encryption type not permitted')

# If the initiator constrains the permitted session enctypes to des3,
# no acceptor subkey will be generated because we can't upgrade to a
# CFX enctype.
test('init des3', 'des3', None,
     tktenc=aes256, tktsession=des3,
     proto='rfc1964', isubkey=des3raw, asubkey=None)

# Force the ticket session key to be rc4, so we can test some subkey
# upgrade cases.  The ticket encryption key remains aes256.
realm.run([kadminl, 'setstr', realm.host_princ, 'session_enctypes', 'rc4'])

# With no arguments, the initiator should send an upgrade list of
# [aes256 aes128 des3] and the acceptor should upgrade to an aes256
# subkey.
test('upgrade noargs', None, None,
     tktenc=aes256, tktsession=rc4,
     proto='cfx', isubkey=rc4, asubkey=aes256)

# If the initiator won't permit rc4 as a session key, it won't be able
# to get a ticket.
test_err('upgrade init aes', 'aes', None, 'no support for encryption type')

# If the initiator permits rc4 but prefers aes128, it will send an
# upgrade list of [aes128] and the acceptor will upgrade to aes128.
test('upgrade init aes128+rc4', 'aes128-cts rc4', None,
     tktenc=aes256, tktsession=rc4,
     proto='cfx', isubkey=rc4, asubkey=aes128)

# If the initiator permits rc4 but prefers des3, it will send an
# upgrade list of [des3], but the acceptor won't generate a subkey
# because des3 isn't a CFX enctype.
test('upgrade init des3+rc4', 'des3 rc4', None,
     tktenc=aes256, tktsession=rc4,
     proto='rfc1964', isubkey=rc4, asubkey=None)

# If the acceptor permits only aes128, subkey negotiation will fail
# because the ticket session key and initiator subkey are
# non-permitted.  (This is unfortunate if the acceptor's restriction
# is only for the sake of the kernel, since we could upgrade to an
# aes128 subkey, but it's the current semantics.)
test_err('upgrade acc aes128', None, 'aes128-cts',
         'Encryption type ArcFour with HMAC/md5 not permitted')

# If the acceptor permits rc4 but prefers aes128, it will negotiate an
# upgrade to aes128.
test('upgrade acc aes128 rc4', None, 'aes128-cts rc4',
     tktenc=aes256, tktsession=rc4,
     proto='cfx', isubkey=rc4, asubkey=aes128)

# In this test, the initiator and acceptor each prefer an AES enctype
# to rc4, but they can't agree on which one, so no subkey is
# generated.
test('upgrade mismatch', 'aes128-cts rc4', 'aes256-cts rc4',
     tktenc=aes256, tktsession=rc4,
     proto='rfc1964', isubkey=rc4, asubkey=None)

success('gss_krb5_set_allowable_enctypes tests')
