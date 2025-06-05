from k5test import *

conf = {'realms': {'$realm': {'supported_enctypes': 'aes256-cts aes128-cts'}}}
realm = K5Realm(create_host=False, kdc_conf=conf)

# Define some server principal names.
princ1 = 'host/1@%s' % realm.realm
princ2 = 'host/2@%s' % realm.realm
princ3 = 'HTTP/3@%s' % realm.realm
princ4 = 'HTTP/4@%s' % realm.realm
matchprinc = 'host/@'
nomatchprinc = 'x/@'
realm.addprinc(princ1)
realm.addprinc(princ2)
realm.addprinc(princ3)

def test(tserver, server, expected):
    args = ['./rdreq', tserver]
    if server is not None:
        args += [server]
    out = realm.run(args)
    if out.strip() != expected:
        fail('unexpected rdreq output')


# No keytab present.
mark('no keytab')
nokeytab_err = "45 Key table file '%s' not found" % realm.keytab
test(princ1, None, nokeytab_err)
test(princ1, princ1, nokeytab_err)
test(princ1, matchprinc, nokeytab_err)

# Keytab present, successful decryption.
mark('success')
realm.extract_keytab(princ1, realm.keytab)
test(princ1, None, '0 success')
test(princ1, princ1, '0 success')
test(princ1, matchprinc, '0 success')

# Explicit server principal not found in keytab.
mark('explicit server not found')
test(princ2, princ2, '45 No key table entry found for host/2@KRBTEST.COM')

# Matching server principal does not match any entries in keytab (with
# and without ticket server present in keytab).
mark('matching server')
nomatch_err = '45 Server principal x/@ does not match any keys in keytab'
test(princ1, nomatchprinc, nomatch_err)
test(princ2, nomatchprinc, nomatch_err)

# Ticket server does not match explicit server principal (with and
# without ticket server present in keytab).
mark('ticket server mismatch')
test(princ1, princ2, '45 No key table entry found for host/2@KRBTEST.COM')
test(princ2, princ1,
     '35 Cannot decrypt ticket for host/2@KRBTEST.COM using keytab key for '
     'host/1@KRBTEST.COM')

# Ticket server not found in keytab during iteration.
mark('ticket server not found')
test(princ2, None,
     '35 Request ticket server host/2@KRBTEST.COM not found in keytab '
     '(ticket kvno 1)')

# Ticket server found in keytab but is not matched by server principal
# (but other principals in keytab do match).
mark('ticket server mismatch (matching)')
realm.extract_keytab(princ3, realm.keytab)
test(princ3, matchprinc,
     '35 Request ticket server HTTP/3@KRBTEST.COM found in keytab but does '
     'not match server principal host/@')

# Service ticket is out of date.
mark('outdated service ticket')
os.remove(realm.keytab)
realm.run([kadminl, 'ktadd', princ1])
test(princ1, None,
     '44 Request ticket server host/1@KRBTEST.COM kvno 1 not found in keytab; '
     'ticket is likely out of date')
test(princ1, princ1,
     '44 Cannot find key for host/1@KRBTEST.COM kvno 1 in keytab')

# kvno mismatch due to ticket principal mismatch with explicit server.
mark('ticket server mismatch (kvno)')
test(princ2, princ1,
     '35 Cannot find key for host/1@KRBTEST.COM kvno 1 in keytab (request '
     'ticket server host/2@KRBTEST.COM)')

# Keytab is out of date.
mark('outdated keytab')
realm.run([kadminl, 'cpw', '-randkey', princ1])
realm.kinit(realm.user_princ, password('user'))
test(princ1, None,
     '44 Request ticket server host/1@KRBTEST.COM kvno 3 not found in keytab; '
     'keytab is likely out of date')
test(princ1, princ1,
     '44 Cannot find key for host/1@KRBTEST.COM kvno 3 in keytab')

# Ticket server and kvno found but not with ticket enctype.
mark('missing enctype')
os.remove(realm.keytab)
realm.extract_keytab(princ1, realm.keytab)
pkeytab = realm.keytab + '.partial'
realm.run([ktutil], input=('rkt %s\ndelent 1\nwkt %s\n' %
                           (realm.keytab, pkeytab)))
os.rename(pkeytab, realm.keytab)
realm.run([klist, '-ke'])
test(princ1, None,
     '44 Request ticket server host/1@KRBTEST.COM kvno 3 found in keytab but '
     'not with enctype aes256-cts')
# This is a bad code (KRB_AP_ERR_NOKEY) and message, because
# krb5_kt_get_entry returns the same result for this and not finding
# the principal at all.  But it's an uncommon case; GSSAPI apps
# usually use a matching principal and missing key enctypes are rare.
test(princ1, princ1, '45 No key table entry found for host/1@KRBTEST.COM')

# Ticket server, kvno, and enctype matched, but key does not work.
mark('wrong key')
realm.run([kadminl, 'cpw', '-randkey', princ1])
realm.run([kadminl, 'modprinc', '-kvno', '3', princ1])
os.remove(realm.keytab)
realm.extract_keytab(princ1, realm.keytab)
test(princ1, None,
     '31 Request ticket server host/1@KRBTEST.COM kvno 3 enctype aes256-cts '
     'found in keytab but cannot decrypt ticket')
test(princ1, princ1,
     '31 Cannot decrypt ticket for host/1@KRBTEST.COM using keytab key for '
     'host/1@KRBTEST.COM')

# Test that aliases work.  The ticket server (princ4) isn't present in
# keytab, but there is a usable princ1 entry with the same key.
mark('aliases')
realm.run([kadminl, 'renprinc', princ1, princ4])
test(princ4, None, '0 success')
test(princ4, princ1, '0 success')
test(princ4, matchprinc, '0 success')

success('krb5_rd_req tests')
