from k5test import *

offline = (len(args) > 0 and args[0] != "no")

conf = {'libdefaults': {'dns_canonicalize_hostname': 'true'},
        'domain_realm': {'kerberos.org': 'R1',
                         'example.com': 'R2',
                         'mit.edu': 'R3'}}
no_rdns_conf = {'libdefaults': {'rdns': 'false'}}
no_canon_conf = {'libdefaults': {'dns_canonicalize_hostname': 'false',
                                 'qualify_shortname': 'example.com'}}
fallback_canon_conf = {'libdefaults':
                       {'rdns': 'false',
                        'dns_canonicalize_hostname': 'fallback'}}

realm = K5Realm(realm='R1', create_host=False, krb5_conf=conf)
no_rdns = realm.special_env('no_rdns', False, krb5_conf=no_rdns_conf)
no_canon = realm.special_env('no_canon', False, krb5_conf=no_canon_conf)
fallback_canon = realm.special_env('fallback_canon', False,
                                   krb5_conf=fallback_canon_conf)

def testbase(host, nametype, princhost, princrealm, env=None):
    # Run the sn2princ harness with a specified host and name type and
    # the fixed service string 'svc', and compare the result to the
    # expected hostname and realm part.
    out = realm.run(['./s2p', host, 'SVC', nametype], env=env).rstrip()
    expected = 'SVC/%s@%s' % (princhost, princrealm)
    if out != expected:
        fail('Expected %s, got %s' % (expected, out))

def test(host, princhost, princrealm):
    # Test with the host-based name type with canonicalization enabled.
    testbase(host, 'srv-hst', princhost, princrealm)

def testnc(host, princhost, princrealm):
    # Test with the host-based name type with canonicalization disabled.
    testbase(host, 'srv-hst', princhost, princrealm, env=no_canon)

def testnr(host, princhost, princrealm):
    # Test with the host-based name type with reverse lookup disabled.
    testbase(host, 'srv-hst', princhost, princrealm, env=no_rdns)

def testu(host, princhost, princrealm):
    # Test with the unknown name type.
    testbase(host, 'unknown', princhost, princrealm)

def testfc(host, princhost, princrealm):
    # Test with the host-based name type with canonicalization fallback.
    testbase(host, 'srv-hst', princhost, princrealm, env=fallback_canon)

# With the unknown principal type, we do not canonicalize or downcase,
# but we do remove a trailing period and look up the realm.
mark('unknown type')
testu('ptr-mismatch.kerberos.org', 'ptr-mismatch.kerberos.org', 'R1')
testu('Example.COM', 'Example.COM', 'R2')
testu('abcde', 'abcde', '')

# A ':port' or ':instance' trailer should be ignored for realm lookup.
# If there is more than one colon in the name, we assume it's an IPv6
# address and don't treat it as having a trailer.
mark('port trailer')
testu('example.com.:123', 'example.com.:123', 'R2')
testu('Example.COM:xyZ', 'Example.COM:xyZ', 'R2')
testu('example.com.::123', 'example.com.::123', '')

# With dns_canonicalize_hostname=false, we downcase and remove
# trailing dots but do not canonicalize the hostname.
# Single-component names are qualified with the configured suffix
# (defaulting to the first OS search domain, but Python cannot easily
# retrieve that value so we don't test it).  Trailers do not get
# downcased.
mark('dns_canonicalize_host=false')
testnc('ptr-mismatch.kerberos.org', 'ptr-mismatch.kerberos.org', 'R1')
testnc('Example.COM', 'example.com', 'R2')
testnc('abcde', 'abcde.example.com', 'R2')
testnc('example.com.:123', 'example.com:123', 'R2')
testnc('Example.COM:xyZ', 'example.com:xyZ', 'R2')
testnc('example.com.::123', 'example.com.::123', '')

if offline:
    skip_rest('sn2princ tests', 'offline mode requested')

# For the online tests, we rely on ptr-mismatch.kerberos.org forward
# and reverse resolving to these names.
oname = 'ptr-mismatch.kerberos.org'
fname = 'www.kerberos.org'

# Test fallback canonicalization krb5_sname_to_principal() results.
mark('dns_canonicalize_host=fallback')
testfc(oname, oname, '')

# Verify forward resolution before testing for it.
try:
    ai = socket.getaddrinfo(oname, None, 0, 0, 0, socket.AI_CANONNAME)
except socket.gaierror:
    skip_rest('sn2princ tests', 'cannot forward resolve %s' % oname)
(family, socktype, proto, canonname, sockaddr) = ai[0]
if canonname.lower() != fname:
    skip_rest('sn2princ tests',
              '%s forward resolves to %s, not %s' % (oname, canonname, fname))

# Test fallback canonicalization in krb5_get_credentials().
oprinc = 'host/' + oname
fprinc = 'host/' + fname
shutil.copy(realm.ccache, realm.ccache + '.save')
# Test that we only try fprinc once if we enter it as input.
out, trace = realm.run(['./gcred', 'srv-hst', fprinc + '@'],
                       env=fallback_canon, expected_code=1, return_trace=True)
msg = 'Requesting tickets for %s@R1, referrals on' % fprinc
if trace.count(msg) != 1:
    fail('Expected one try for %s' % fprinc)
# Create fprinc, and verify that we get it as the canonicalized
# fallback for oprinc.
realm.addprinc(fprinc)
msgs = ('Getting credentials user@R1 -> %s@ using' % oprinc,
        'Requesting tickets for %s@R1' % oprinc,
        'Requesting tickets for %s@R1' % fprinc,
        'Received creds for desired service %s@R1' % fprinc)
realm.run(['./gcred', 'srv-hst', oprinc + '@'], env=fallback_canon,
          expected_msg=fprinc, expected_trace=msgs)
realm.addprinc(oprinc)
# oprinc now exists, but we still get the fprinc ticket from the cache.
realm.run(['./gcred', 'srv-hst', oprinc + '@'], env=fallback_canon,
          expected_msg=fprinc)
# Without the cached result, we should get oprinc in preference to fprinc.
os.rename(realm.ccache + '.save', realm.ccache)
realm.run(['./gcred', 'srv-hst', oprinc], env=fallback_canon,
          expected_msg=oprinc)

# Test fallback canonicalization for krb5_rd_req().
realm.run([kadminl, 'ktadd', fprinc])
msgs = ('Decrypted AP-REQ with server principal %s@R1' % fprinc,
        'AP-REQ ticket: user@R1 -> %s@R1' % fprinc)
realm.run(['./rdreq', fprinc, oprinc + '@'], env=fallback_canon,
          expected_trace=msgs)

# Test fallback canonicalization for getting initial creds with a keytab.
msgs = ('Getting initial credentials for %s@' % oprinc,
        'Found entries for %s@R1 in keytab' % fprinc,
        'Retrieving %s@R1 from ' % fprinc)
realm.run(['./icred', '-k', realm.keytab, '-S', 'host', oname],
          env=fallback_canon, expected_trace=msgs)

# Test forward-only canonicalization (rdns=false).
mark('rdns=false')
testnr(oname, fname, 'R1')
testnr(oname + ':123', fname + ':123', 'R1')
testnr(oname + ':xyZ', fname + ':xyZ', 'R1')

# Verify reverse resolution before testing for it.
try:
    names = socket.getnameinfo(sockaddr, socket.NI_NAMEREQD)
except socket.gaierror:
    skip_rest('reverse sn2princ tests', 'cannot reverse resolve %s' % oname)
rname = names[0].lower()
if rname == fname:
    skip_rest('reverse sn2princ tests',
              '%s reverse resolves to %s '
              'which should be different from %s' % (oname, rname, fname))

# Test default canonicalization (forward and reverse lookup).
mark('default')
test(oname, rname, 'R3')
test(oname + ':123', rname + ':123', 'R3')
test(oname + ':xyZ', rname + ':xyZ', 'R3')

success('krb5_sname_to_principal tests')
