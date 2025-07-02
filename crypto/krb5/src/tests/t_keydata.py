from k5test import *

realm = K5Realm(create_user=False, create_host=False)

# Create a principal with no keys.
realm.run([kadminl, 'addprinc', '-nokey', 'user'])
realm.run([kadminl, 'getprinc', 'user'], expected_msg='Number of keys: 0')

# Change its password and check the resulting kvno.
realm.run([kadminl, 'cpw', '-pw', 'password', 'user'])
realm.run([kadminl, 'getprinc', 'user'], expected_msg='vno 1')

# Delete all of its keys.
realm.run([kadminl, 'purgekeys', '-all', 'user'])
realm.run([kadminl, 'getprinc', 'user'], expected_msg='Number of keys: 0')

# Randomize its keys and check the resulting kvno.
realm.run([kadminl, 'cpw', '-randkey', 'user'])
realm.run([kadminl, 'getprinc', 'user'], expected_msg='vno 1')

# Return true if patype appears to have been received in a hint list
# from a KDC error message, based on the trace file fname.
def preauth_type_received(trace, patype):
    found = False
    for line in trace.splitlines():
        if 'Processing preauth types:' in line:
            ind = line.find('types:')
            patypes = line[ind + 6:].split(', ')
            if str(patype) in patypes:
                found = True
    return found

# Make sure the KDC doesn't offer encrypted timestamp for a principal
# with no keys.
realm.run([kadminl, 'purgekeys', '-all', 'user'])
realm.run([kadminl, 'modprinc', '+requires_preauth', 'user'])
out, trace = realm.run([kinit, 'user'], expected_code=1, return_trace=True)
if preauth_type_received(trace, 2):
    fail('encrypted timestamp')

# Make sure it doesn't offer encrypted challenge either.
realm.run([kadminl, 'addprinc', '-pw', 'fast', 'armor'])
realm.kinit('armor', 'fast')
out, trace = realm.run([kinit, '-T', realm.ccache, 'user'], expected_code=1,
                       return_trace=True)
if preauth_type_received(trace, 138):
    fail('encrypted challenge')

success('Key data tests')
