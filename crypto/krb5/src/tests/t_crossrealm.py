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

def test_kvno(r, princ, test, env=None):
    r.run([kvno, princ], env=env, expected_msg=princ)


def stop(*realms):
    for r in realms:
        r.stop()


# Verify that the princs appear as the service principals in the klist
# output for the realm r, in order.
def check_klist(r, princs):
    out = r.run([klist])
    count = 0
    seen_header = False
    for l in out.split('\n'):
        if l.startswith('Valid starting'):
            seen_header = True
            continue
        if not seen_header or l == '':
            continue
        if count >= len(princs):
            fail('too many entries in klist output')
        svcprinc = l.split()[4]
        if svcprinc != princs[count]:
            fail('saw service princ %s in klist output, expected %s' %
                 (svcprinc, princs[count]))
        count += 1
    if count != len(princs):
        fail('not enough entries in klist output')


def tgt(r1, r2):
    return 'krbtgt/%s@%s' % (r1.realm, r2.realm)


# Basic two-realm test with cross TGTs in both directions.
mark('two realms')
r1, r2 = cross_realms(2)
test_kvno(r1, r2.host_princ, 'basic r1->r2')
check_klist(r1, (tgt(r1, r1), tgt(r2, r1), r2.host_princ))
test_kvno(r2, r1.host_princ, 'basic r2->r1')
check_klist(r2, (tgt(r2, r2), tgt(r1, r2), r1.host_princ))
stop(r1, r2)

# Test the KDC domain walk for hierarchically arranged realms.  The
# client in A.X will ask for a cross TGT to B.X, but A.X's KDC only
# has a TGT for the intermediate realm X, so it will return that
# instead.  The client will use that to get a TGT for B.X.
mark('hierarchical realms')
r1, r2, r3 = cross_realms(3, xtgts=((0,1), (1,2)), 
                          args=({'realm': 'A.X'}, {'realm': 'X'},
                                {'realm': 'B.X'}))
test_kvno(r1, r3.host_princ, 'KDC domain walk')
check_klist(r1, (tgt(r1, r1), r3.host_princ))

# Test start_realm in this setup.
r1.run([kvno, '--out-cache', r1.ccache, r2.krbtgt_princ])
r1.run([klist, '-C'], expected_msg='config: start_realm = X')
msgs = ('Requesting TGT krbtgt/B.X@X using TGT krbtgt/X@X',
        'Received TGT for service realm: krbtgt/B.X@X')
r1.run([kvno, r3.host_princ], expected_trace=msgs)

stop(r1, r2, r3)

# Test client capaths.  The client in A will ask for a cross TGT to D,
# but A's KDC won't have it and won't know an intermediate to return.
# The client will walk its A->D capaths to get TGTs for B, then C,
# then D.  The KDCs for C and D need capaths settings to avoid failing
# transited checks, including a capaths for A->C.
mark('client capaths')
capaths = {'capaths': {'A': {'D': ['B', 'C'], 'C': 'B'}}}
r1, r2, r3, r4 = cross_realms(4, xtgts=((0,1), (1,2), (2,3)),
                              args=({'realm': 'A'},
                                    {'realm': 'B'},
                                    {'realm': 'C', 'krb5_conf': capaths},
                                    {'realm': 'D', 'krb5_conf': capaths}))
r1client = r1.special_env('client', False, krb5_conf=capaths)
test_kvno(r1, r4.host_princ, 'client capaths', r1client)
check_klist(r1, (tgt(r1, r1), tgt(r2, r1), tgt(r3, r2), tgt(r4, r3),
                 r4.host_princ))
stop(r1, r2, r3, r4)

# Test KDC capaths.  The KDCs for A and B have appropriate capaths
# settings to determine intermediate TGTs to return, but the client
# has no idea.
mark('kdc capaths')
capaths = {'capaths': {'A': {'D': ['B', 'C'], 'C': 'B'}, 'B': {'D': 'C'}}}
r1, r2, r3, r4 = cross_realms(4, xtgts=((0,1), (1,2), (2,3)),
                              args=({'realm': 'A', 'krb5_conf': capaths},
                                    {'realm': 'B', 'krb5_conf': capaths},
                                    {'realm': 'C', 'krb5_conf': capaths},
                                    {'realm': 'D', 'krb5_conf': capaths}))
r1client = r1.special_env('client', False, krb5_conf={'capaths': None})
test_kvno(r1, r4.host_princ, 'KDC capaths', r1client)
check_klist(r1, (tgt(r1, r1), r4.host_princ))
stop(r1, r2, r3, r4)

# A capaths value of '.' should enforce direct cross-realm, with no
# intermediate.
mark('direct cross-realm enforcement')
capaths = {'capaths': {'A.X': {'B.X': '.'}}}
r1, r2, r3 = cross_realms(3, xtgts=((0,1), (1,2)),
                          args=({'realm': 'A.X', 'krb5_conf': capaths},
                                {'realm': 'X'}, {'realm': 'B.X'}))
r1.run([kvno, r3.host_princ], expected_code=1,
       expected_msg='Server krbtgt/B.X@A.X not found in Kerberos database')
stop(r1, r2, r3)

# Test transited error.  The KDC for C does not recognize B as an
# intermediate realm for A->C, so it refuses to issue a service
# ticket.
mark('transited error (three realms)')
capaths = {'capaths': {'A': {'C': 'B'}}}
r1, r2, r3 = cross_realms(3, xtgts=((0,1), (1,2)),
                          args=({'realm': 'A', 'krb5_conf': capaths},
                                {'realm': 'B'}, {'realm': 'C'}))
r1.run([kvno, r3.host_princ], expected_code=1,
       expected_msg='KDC policy rejects request')
check_klist(r1, (tgt(r1, r1), tgt(r3, r2)))
stop(r1, r2, r3)

# Test server transited checking.  The KDC for C recognizes B as an
# intermediate realm for A->C, but the server environment does not.
# The server should honor the ticket if the transited-policy-checked
# flag is set, but not if it isn't.  (It is only possible for our KDC
# to issue a ticket without the transited-policy-checked flag with
# reject_bad_transit=false.)
mark('server transited checking')
capaths = {'capaths': {'A': {'C': 'B'}}}
noreject = {'realms': {'$realm': {'reject_bad_transit': 'false'}}}
r1, r2, r3 = cross_realms(3, xtgts=((0,1), (1,2)),
                          args=({'realm': 'A', 'krb5_conf': capaths},
                                {'realm': 'B'},
                                {'realm': 'C', 'krb5_conf': capaths,
                                 'kdc_conf': noreject}))
r3server = r3.special_env('server', False, krb5_conf={'capaths': None})
# Process a ticket with the transited-policy-checked flag set.
shutil.copy(r1.ccache, r1.ccache + '.copy')
r1.run(['./gcred', 'principal', r3.host_princ])
os.rename(r1.ccache, r3.ccache)
r3.run(['./rdreq', r3.host_princ], env=r3server, expected_msg='0 success')
# Try again with the transited-policy-checked flag unset.
os.rename(r1.ccache + '.copy', r1.ccache)
r1.run(['./gcred', '-t', 'principal', r3.host_princ])
os.rename(r1.ccache, r3.ccache)
r3.run(['./rdreq', r3.host_princ], env=r3server,
       expected_msg='43 Illegal cross-realm ticket')
stop(r1, r2, r3)

# Test a four-realm scenario.  This test used to result in an "Illegal
# cross-realm ticket" error as the KDC for D would refuse to process
# the cross-realm ticket from C.  Now that we honor the
# transited-policy-checked flag in krb5_rd_req(), it instead issues a
# policy error as in the three-realm scenario.
mark('transited error (four realms)')
capaths = {'capaths': {'A': {'D': ['B', 'C'], 'C': 'B'}, 'B': {'D': 'C'}}}
r1, r2, r3, r4 = cross_realms(4, xtgts=((0,1), (1,2), (2,3)),
                              args=({'realm': 'A', 'krb5_conf': capaths},
                                    {'realm': 'B', 'krb5_conf': capaths},
                                    {'realm': 'C', 'krb5_conf': capaths},
                                    {'realm': 'D'}))
r1.run([kvno, r4.host_princ], expected_code=1,
       expected_msg='KDC policy rejects request')
check_klist(r1, (tgt(r1, r1), tgt(r4, r3)))
stop(r1, r2, r3, r4)

success('Cross-realm tests')
