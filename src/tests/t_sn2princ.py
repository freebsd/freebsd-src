#!/usr/bin/python
from k5test import *

offline = (len(args) > 0 and args[0] != "no")

conf = {'domain_realm': {'kerberos.org': 'R1',
                         'example.com': 'R2',
                         'mit.edu': 'R3'}}
no_rdns_conf = {'libdefaults': {'rdns': 'false'}}
no_canon_conf = {'libdefaults': {'dns_canonicalize_hostname': 'false'}}

realm = K5Realm(create_kdb=False, krb5_conf=conf)
no_rdns = realm.special_env('no_rdns', False, krb5_conf=no_rdns_conf)
no_canon = realm.special_env('no_canon', False, krb5_conf=no_canon_conf)

def testbase(host, nametype, princhost, princrealm, env=None):
    # Run the sn2princ harness with a specified host and name type and
    # the fixed service string 'svc', and compare the result to the
    # expected hostname and realm part.
    out = realm.run(['./s2p', host, 'SVC', nametype], env=env).rstrip()
    expected = 'SVC/%s@%s' % (princhost, princrealm)
    if out != expected:
        fail('Expected %s, got %s' % (expected, out))

def test(host, princhost, princrealm):
    # Test with the host-based name type in the default environment.
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

# With the unknown principal type, we do not canonicalize or downcase,
# but we do remove a trailing period and look up the realm.
testu('ptr-mismatch.kerberos.org', 'ptr-mismatch.kerberos.org', 'R1')
testu('Example.COM', 'Example.COM', 'R2')
testu('abcde', 'abcde', '')

# A ':port' or ':instance' trailer should be ignored for realm lookup.
# If there is more than one colon in the name, we assume it's an IPv6
# address and don't treat it as having a trailer.
testu('example.com.:123', 'example.com.:123', 'R2')
testu('Example.COM:xyZ', 'Example.COM:xyZ', 'R2')
testu('example.com.::123', 'example.com.::123', '')

# With dns_canonicalize_hostname=false, we downcase and remove
# trailing dots but do not canonicalize the hostname.  Trailers do not
# get downcased.
testnc('ptr-mismatch.kerberos.org', 'ptr-mismatch.kerberos.org', 'R1')
testnc('Example.COM', 'example.com', 'R2')
testnc('abcde', 'abcde', '')
testnc('example.com.:123', 'example.com:123', 'R2')
testnc('Example.COM:xyZ', 'example.com:xyZ', 'R2')
testnc('example.com.::123', 'example.com.::123', '')

if offline:
    skip_rest('sn2princ tests', 'offline mode requested')

# For the online tests, we rely on ptr-mismatch.kerberos.org forward
# and reverse resolving to these names.
oname = 'ptr-mismatch.kerberos.org'
fname = 'www.kerberos.org'

# Verify forward resolution before testing for it.
try:
    ai = socket.getaddrinfo(oname, None, 0, 0, 0, socket.AI_CANONNAME)
except socket.gaierror:
    skip_rest('sn2princ tests', 'cannot forward resolve %s' % oname)
(family, socktype, proto, canonname, sockaddr) = ai[0]
if canonname.lower() != fname:
    skip_rest('sn2princ tests',
              '%s forward resolves to %s, not %s' % (oname, canonname, fname))

# Test forward-only canonicalization (rdns=false).
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
test(oname, rname, 'R3')
test(oname + ':123', rname + ':123', 'R3')
test(oname + ':xyZ', rname + ':xyZ', 'R3')

success('krb5_sname_to_principal tests')
