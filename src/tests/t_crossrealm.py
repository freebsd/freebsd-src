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
r1, r2, r3 = cross_realms(3, xtgts=((0,1), (1,2)), 
                          args=({'realm': 'A.X'}, {'realm': 'X'},
                                {'realm': 'B.X'}))
test_kvno(r1, r3.host_princ, 'KDC domain walk')
check_klist(r1, (tgt(r1, r1), r3.host_princ))
stop(r1, r2, r3)

# Test client capaths.  The client in A will ask for a cross TGT to D,
# but A's KDC won't have it and won't know an intermediate to return.
# The client will walk its A->D capaths to get TGTs for B, then C,
# then D.  The KDCs for C and D need capaths settings to avoid failing
# transited checks, including a capaths for A->C.
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
capaths = {'capaths': {'A': {'D': ['B', 'C'], 'C': 'B'}, 'B': {'D': 'C'}}}
r1, r2, r3, r4 = cross_realms(4, xtgts=((0,1), (1,2), (2,3)),
                              args=({'realm': 'A', 'krb5_conf': capaths},
                                    {'realm': 'B', 'krb5_conf': capaths},
                                    {'realm': 'C', 'krb5_conf': capaths},
                                    {'realm': 'D', 'krb5_conf': capaths}))
test_kvno(r1, r4.host_princ, 'KDC capaths')
check_klist(r1, (tgt(r1, r1), tgt(r4, r3), r4.host_princ))
stop(r1, r2, r3, r4)

# Test transited error.  The KDC for C does not recognize B as an
# intermediate realm for A->C, so it refuses to issue a service
# ticket.
capaths = {'capaths': {'A': {'C': 'B'}}}
r1, r2, r3 = cross_realms(3, xtgts=((0,1), (1,2)),
                          args=({'realm': 'A', 'krb5_conf': capaths},
                                {'realm': 'B'}, {'realm': 'C'}))
r1.run([kvno, r3.host_princ], expected_code=1,
       expected_msg='KDC policy rejects request')
check_klist(r1, (tgt(r1, r1), tgt(r3, r2)))
stop(r1, r2, r3)

# Test a different kind of transited error.  The KDC for D does not
# recognize B as an intermediate realm for A->C, so it refuses to
# verify the krbtgt/C@B ticket in the TGS AP-REQ.
capaths = {'capaths': {'A': {'D': ['B', 'C'], 'C': 'B'}, 'B': {'D': 'C'}}}
r1, r2, r3, r4 = cross_realms(4, xtgts=((0,1), (1,2), (2,3)),
                              args=({'realm': 'A', 'krb5_conf': capaths},
                                    {'realm': 'B', 'krb5_conf': capaths},
                                    {'realm': 'C', 'krb5_conf': capaths},
                                    {'realm': 'D'}))
r1.run([kvno, r4.host_princ], expected_code=1,
       expected_msg='Illegal cross-realm ticket')
check_klist(r1, (tgt(r1, r1), tgt(r4, r3)))
stop(r1, r2, r3, r4)

success('Cross-realm tests')
