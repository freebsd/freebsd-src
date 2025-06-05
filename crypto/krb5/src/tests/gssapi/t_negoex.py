from k5test import *

# The next arc after 2.25 is supposed to be a single-integer UUID, but
# since our gss_str_to_oid() can't handle arc values that don't fit in
# an unsigned long, we use random unsigned 32-bit integers instead.
# The final octet if the OID encoding will be used to identify the
# mechanism when changing the behavior of just one mech.
nxtest_oid1 = '2.25.1414534758' # final octet is 102 (0x66)
nxtest_oid2 = '2.25.1175737388' # final octet is 44 (0x2C)
nxtest_path = os.path.join(buildtop, 'plugins', 'gssapi', 'negoextest',
                           'gss_negoextest.so')

# Test gss_add_cred().
realm = K5Realm(create_kdb=False)
with open(realm.gss_mech_config, 'w') as f:
    f.write('negoextest %s %s\n' % (nxtest_oid1, nxtest_path))
    f.write('negoextest %s %s\n' % (nxtest_oid2, nxtest_path))

def test(envvars, **kw):
    # Python 3.5: e = {**realm.env, **vars}
    e = realm.env.copy()
    e.update(envvars)
    realm.run(['./t_context', 'h:host'], env=e, **kw)

# Test varying numbers of hops, and spot-check that messages are sent
# in the appropriate sequence.

mark('One hop')
msgs = ('sending [0]INITIATOR_NEGO: c0a28569-66ac-0000-0000-000000000000 '
        'd1b08469-2ca8-0000-0000-000000000000',
        'sending [1]INITIATOR_META_DATA: c0a28569-66ac',
        'sending [2]INITIATOR_META_DATA: d1b08469-2ca8',
        'sending [3]AP_REQUEST: c0a28569-66ac',
        'sending [4]VERIFY: c0a28569-66ac',
        'received [0]INITIATOR_NEGO: c0a28569-66ac-0000-0000-000000000000 '
        'd1b08469-2ca8-0000-0000-000000000000',
        'received [1]INITIATOR_META_DATA: c0a28569-66ac',
        'received [2]INITIATOR_META_DATA: d1b08469-2ca8',
        'received [3]AP_REQUEST: c0a28569-66ac',
        'received [4]VERIFY: c0a28569-66ac',
        'sending [5]ACCEPTOR_NEGO: c0a28569-66ac-0000-0000-000000000000 '
        'd1b08469-2ca8-0000-0000-000000000000',
        'sending [6]ACCEPTOR_META_DATA: c0a28569-66ac',
        'sending [7]ACCEPTOR_META_DATA: d1b08469-2ca8',
        'sending [8]VERIFY: c0a28569-66ac',
        'received [5]ACCEPTOR_NEGO: c0a28569-66ac-0000-0000-000000000000 '
        'd1b08469-2ca8-0000-0000-000000000000',
        'received [6]ACCEPTOR_META_DATA: c0a28569-66ac',
        'received [7]ACCEPTOR_META_DATA: d1b08469-2ca8',
        'received [8]VERIFY: c0a28569-66ac')
test({'HOPS': '1'}, expected_trace=msgs)

mark('Two hops')
msgs = ('sending [7]CHALLENGE', 'sending [8]VERIFY', 'received [8]VERIFY',
        'sending [9]VERIFY')
test({'HOPS': '2'}, expected_trace=msgs)

mark('Three hops')
msgs = ('sending [8]AP_REQUEST', 'sending [9]VERIFY', 'received [8]AP_REQUEST',
        'sending [10]VERIFY')
test({'HOPS': '3'}, expected_trace=msgs)

mark('Four hops')
msgs = ('sending [9]CHALLENGE', 'sending [10]VERIFY', 'received [9]CHALLENGE',
        'sending [11]VERIFY')
test({'HOPS': '4'}, expected_trace=msgs)

mark('Early keys, three hops')
msgs = ('sending [4]VERIFY', 'sending [9]VERIFY', 'sending [10]AP_REQUEST')
test({'HOPS': '3', 'KEY': 'always'}, expected_trace=msgs)

mark('Early keys, four hops')
msgs = ('sending [4]VERIFY', 'sending [9]VERIFY', 'sending [10]AP_REQUEST',
        'sending [11]CHALLENGE')
test({'HOPS': '4', 'KEY': 'always'}, expected_trace=msgs)

mark('No keys')
test({'KEY': 'never'}, expected_code=1, expected_msg='No NegoEx verify key')

mark('No optimistic token')
msgs = ('sending [3]ACCEPTOR_NEGO', 'sending [6]AP_REQUEST',
        'sending [7]VERIFY', 'sending [8]VERIFY')
test({'NEGOEX_NO_OPTIMISTIC_TOKEN': ''}, expected_trace=msgs)

mark('First mech initiator query fail')
msgs = ('sending [0]INITIATOR_NEGO: d1b08469-2ca8-0000-0000-000000000000',
        'sending [2]AP_REQUEST', 'sending [3]VERIFY',
        'sending [4]ACCEPTOR_NEGO: d1b08469-2ca8-0000-0000-000000000000',
        'sending [6]VERIFY')
test({'INIT_QUERY_FAIL': '102'}, expected_trace=msgs)

mark('First mech acceptor query fail')
msgs = ('sending [0]INITIATOR_NEGO: c0a28569-66ac-0000-0000-000000000000 '
        'd1b08469-2ca8-0000-0000-000000000000',
        'sending [3]AP_REQUEST: c0a28569-66ac',
        'sending [4]VERIFY: c0a28569-66ac',
        'sending [5]ACCEPTOR_NEGO: d1b08469-2ca8-0000-0000-000000000000',
        'sending [7]AP_REQUEST: d1b08469-2ca8',
        'sending [8]VERIFY: d1b08469-2ca8',
        'sending [9]VERIFY: d1b08469-2ca8')
test({'ACCEPT_QUERY_FAIL': '102'}, expected_trace=msgs)

# Same messages as previous test.
mark('First mech acceptor exchange fail')
test({'ACCEPT_EXCHANGE_FAIL': '102'}, expected_trace=msgs)

# Fail the optimistic mech's gss_exchange_meta_data() in the
# initiator.  Since the acceptor has effectively selected the
# optimistic mech, this causes the authentication to fail.
mark('First mech initiator exchange fail, one hop')
test({'HOPS': '1', 'INIT_EXCHANGE_FAIL': '102'}, expected_code=1,
     expected_msg='No mutually supported NegoEx authentication schemes')
mark('First mech initiator exchange fail, two hops, early keys')
test({'HOPS': '2', 'INIT_EXCHANGE_FAIL': '102', 'KEY': 'always'},
     expected_code=1,
     expected_msg='No mutually supported NegoEx authentication schemes')
mark('First mech initiator exchange fail, two hops')
test({'HOPS': '2', 'INIT_EXCHANGE_FAIL': '102'}, expected_code=1,
     expected_msg='No mutually supported NegoEx authentication schemes')

mark('First mech init_sec_context fail')
msgs = ('sending [0]INITIATOR_NEGO: d1b08469-2ca8-0000-0000-000000000000',
        'sending [2]AP_REQUEST', 'sending [3]VERIFY', 'sending [6]VERIFY')
test({'INIT_FAIL': '102'}, expected_trace=msgs)

mark('First mech accept_sec_context fail')
test({'HOPS': '2', 'ACCEPT_FAIL': '102'}, expected_code=1,
     expected_msg='failure from acceptor')

mark('ALERT from acceptor to initiator')
msgs = ('sending [3]AP_REQUEST', 'sending [4]VERIFY', 'sending [8]CHALLENGE',
        'sending [9]ALERT', 'received [9]ALERT', 'sending [10]AP_REQUEST',
        'sending [11]VERIFY', 'sending [12]VERIFY')
test({'HOPS': '3', 'KEY': 'init-always'}, expected_trace=msgs)

mark('ALERT from initiator to acceptor')
msgs = ('sending [3]AP_REQUEST', 'sending [7]CHALLENGE', 'sending [8]VERIFY',
        'sending [9]AP_REQUEST', 'sending [10]ALERT', 'received [10]ALERT',
        'sending [11]CHALLENGE', 'sending [12]VERIFY', 'sending [13]VERIFY')
test({'HOPS': '4', 'KEY': 'accept-always'}, expected_trace=())

mark('channel bindings')
e = realm.env.copy()
e.update({'HOPS': '1', 'GSS_INIT_BINDING': 'a', 'GSS_ACCEPT_BINDING': 'b'})
# The test mech will verify that the bindings are communicated to the
# mech, but does not set the channel-bound flag.
realm.run(['./t_bindings', '-s', 'h:host', 'a', 'b'], env=e, expected_msg='no')

success('NegoEx tests')
