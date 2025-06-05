# This is a simple KCM test server, used to exercise the KCM ccache
# client code.  It will generally throw an uncaught exception if the
# client sends anything unexpected, so is unsuitable for production.
# (It also imposes no namespace or access constraints, and blocks
# while reading requests and writing responses.)

# This code knows nothing about how to marshal and unmarshal principal
# names and credentials as is required in the KCM protocol; instead,
# it just remembers the marshalled forms and replays them to the
# client when asked.  This works because marshalled creds and
# principal names are always the last part of marshalled request
# arguments, and because we don't need to implement remove_cred (which
# would need to know how to match a cred tag against previously stored
# credentials).

# The following code is useful for debugging if anything appears to be
# going wrong in the server, since daemon output is generally not
# visible in Python test scripts.
#
# import sys, traceback
# def ehook(etype, value, tb):
#     with open('/tmp/exception', 'w') as f:
#         traceback.print_exception(etype, value, tb, file=f)
# sys.excepthook = ehook

import optparse
import select
import socket
import struct
import sys

caches = {}
cache_uuidmap = {}
defname = b'default'
next_unique = 1
next_uuid = 1

class KCMOpcodes(object):
    GEN_NEW = 3
    INITIALIZE = 4
    DESTROY = 5
    STORE = 6
    RETRIEVE = 7
    GET_PRINCIPAL = 8
    GET_CRED_UUID_LIST = 9
    GET_CRED_BY_UUID = 10
    REMOVE_CRED = 11
    GET_CACHE_UUID_LIST = 18
    GET_CACHE_BY_UUID = 19
    GET_DEFAULT_CACHE = 20
    SET_DEFAULT_CACHE = 21
    GET_KDC_OFFSET = 22
    SET_KDC_OFFSET = 23
    GET_CRED_LIST = 13001
    REPLACE = 13002


class KRB5Errors(object):
    KRB5_CC_NOTFOUND = -1765328243
    KRB5_CC_END = -1765328242
    KRB5_CC_NOSUPP = -1765328137
    KRB5_FCC_NOFILE = -1765328189
    KRB5_FCC_INTERNAL = -1765328188


def make_uuid():
    global next_uuid
    uuid = bytes(12) + struct.pack('>L', next_uuid)
    next_uuid = next_uuid + 1
    return uuid


class Cache(object):
    def __init__(self, name):
        self.name = name
        self.princ = None
        self.uuid = make_uuid()
        self.cred_uuids = []
        self.creds = {}
        self.time_offset = 0


def get_cache(name):
    if name in caches:
        return caches[name]
    cache = Cache(name)
    caches[name] = cache
    cache_uuidmap[cache.uuid] = cache
    return cache


def unmarshal_name(argbytes):
    offset = argbytes.find(b'\0')
    return argbytes[0:offset], argbytes[offset+1:]


# Find the bounds of a marshalled principal, returning it and the
# remainder of argbytes.
def extract_princ(argbytes):
    ncomps, rlen = struct.unpack('>LL', argbytes[4:12])
    pos = 12 + rlen
    for i in range(ncomps):
        clen, = struct.unpack('>L', argbytes[pos:pos+4])
        pos += 4 + clen
    return argbytes[0:pos], argbytes[pos:]


# Return true if the marshalled principals p1 and p2 name the same
# principal.
def princ_eq(p1, p2):
    # Ignore the name-types at bytes 0..3.  The remaining bytes should
    # be identical if the principals are the same.
    return p1[4:] == p2[4:]


def op_gen_new(argbytes):
    # Does not actually check for uniqueness.
    global next_unique
    name = b'unique' + str(next_unique).encode('ascii')
    next_unique += 1
    return 0, name + b'\0'


def op_initialize(argbytes):
    name, princ = unmarshal_name(argbytes)
    cache = get_cache(name)
    cache.princ = princ
    cache.cred_uuids = []
    cache.creds = {}
    cache.time_offset = 0
    return 0, b''


def op_destroy(argbytes):
    name, rest = unmarshal_name(argbytes)
    cache = get_cache(name)
    del cache_uuidmap[cache.uuid]
    del caches[name]
    return 0, b''


def op_store(argbytes):
    name, cred = unmarshal_name(argbytes)
    cache = get_cache(name)
    uuid = make_uuid()
    cache.creds[uuid] = cred
    cache.cred_uuids.append(uuid)
    return 0, b''


def op_retrieve(argbytes):
    name, rest = unmarshal_name(argbytes)
    # Ignore the flags at rest[0:4] and the header at rest[4:8].
    # Assume there are client and server creds in the tag and match
    # only against them.
    cprinc, rest = extract_princ(rest[8:])
    sprinc, rest = extract_princ(rest)
    cache = get_cache(name)
    for cred in (cache.creds[u] for u in cache.cred_uuids):
        cred_cprinc, rest = extract_princ(cred)
        cred_sprinc, rest = extract_princ(rest)
        if princ_eq(cred_cprinc, cprinc) and princ_eq(cred_sprinc, sprinc):
            return 0, cred
    return KRB5Errors.KRB5_CC_NOTFOUND, b''


def op_get_principal(argbytes):
    name, rest = unmarshal_name(argbytes)
    cache = get_cache(name)
    if cache.princ is None:
        return KRB5Errors.KRB5_FCC_NOFILE, b''
    return 0, cache.princ + b'\0'


def op_get_cred_uuid_list(argbytes):
    name, rest = unmarshal_name(argbytes)
    cache = get_cache(name)
    return 0, b''.join(cache.cred_uuids)


def op_get_cred_by_uuid(argbytes):
    name, uuid = unmarshal_name(argbytes)
    cache = get_cache(name)
    if uuid not in cache.creds:
        return KRB5Errors.KRB5_CC_END, b''
    return 0, cache.creds[uuid]


def op_remove_cred(argbytes):
    return KRB5Errors.KRB5_CC_NOSUPP, b''


def op_get_cache_uuid_list(argbytes):
    return 0, b''.join(cache_uuidmap.keys())


def op_get_cache_by_uuid(argbytes):
    uuid = argbytes
    if uuid not in cache_uuidmap:
        return KRB5Errors.KRB5_CC_END, b''
    return 0, cache_uuidmap[uuid].name + b'\0'


def op_get_default_cache(argbytes):
    return 0, defname + b'\0'


def op_set_default_cache(argbytes):
    global defname
    defname, rest = unmarshal_name(argbytes)
    return 0, b''


def op_get_kdc_offset(argbytes):
    name, rest = unmarshal_name(argbytes)
    cache = get_cache(name)
    return 0, struct.pack('>l', cache.time_offset)


def op_set_kdc_offset(argbytes):
    name, obytes = unmarshal_name(argbytes)
    cache = get_cache(name)
    cache.time_offset, = struct.unpack('>l', obytes)
    return 0, b''


def op_get_cred_list(argbytes):
    name, rest = unmarshal_name(argbytes)
    cache = get_cache(name)
    creds = [cache.creds[u] for u in cache.cred_uuids]
    return 0, (struct.pack('>L', len(creds)) +
               b''.join(struct.pack('>L', len(c)) + c for c in creds))


def op_replace(argbytes):
    name, rest = unmarshal_name(argbytes)
    offset, = struct.unpack('>L', rest[0:4])
    princ, rest = extract_princ(rest[4:])
    ncreds, = struct.unpack('>L', rest[0:4])
    rest = rest[4:]
    creds = []
    for i in range(ncreds):
        len, = struct.unpack('>L', rest[0:4])
        creds.append(rest[4:4+len])
        rest = rest[4+len:]

    cache = get_cache(name)
    cache.princ = princ
    cache.cred_uuids = []
    cache.creds = {}
    cache.time_offset = offset
    for i in range(ncreds):
        uuid = make_uuid()
        cache.creds[uuid] = creds[i]
        cache.cred_uuids.append(uuid)

    return 0, b''


ophandlers = {
    KCMOpcodes.GEN_NEW : op_gen_new,
    KCMOpcodes.INITIALIZE : op_initialize,
    KCMOpcodes.DESTROY : op_destroy,
    KCMOpcodes.STORE : op_store,
    KCMOpcodes.RETRIEVE : op_retrieve,
    KCMOpcodes.GET_PRINCIPAL : op_get_principal,
    KCMOpcodes.GET_CRED_UUID_LIST : op_get_cred_uuid_list,
    KCMOpcodes.GET_CRED_BY_UUID : op_get_cred_by_uuid,
    KCMOpcodes.REMOVE_CRED : op_remove_cred,
    KCMOpcodes.GET_CACHE_UUID_LIST : op_get_cache_uuid_list,
    KCMOpcodes.GET_CACHE_BY_UUID : op_get_cache_by_uuid,
    KCMOpcodes.GET_DEFAULT_CACHE : op_get_default_cache,
    KCMOpcodes.SET_DEFAULT_CACHE : op_set_default_cache,
    KCMOpcodes.GET_KDC_OFFSET : op_get_kdc_offset,
    KCMOpcodes.SET_KDC_OFFSET : op_set_kdc_offset,
    KCMOpcodes.GET_CRED_LIST : op_get_cred_list,
    KCMOpcodes.REPLACE : op_replace
}

# Read and respond to a request from the socket s.
def service_request(s):
    lenbytes = b''
    while len(lenbytes) < 4:
        lenbytes += s.recv(4 - len(lenbytes))
        if lenbytes == b'':
                return False

    reqlen, = struct.unpack('>L', lenbytes)
    req = b''
    while len(req) < reqlen:
        req += s.recv(reqlen - len(req))

    majver, minver, op = struct.unpack('>BBH', req[:4])
    argbytes = req[4:]

    if op in ophandlers:
        code, payload = ophandlers[op](argbytes)
    else:
        code, payload = KRB5Errors.KRB5_FCC_INTERNAL, b''

    # The KCM response is the code (4 bytes) and the response payload.
    # The Heimdal IPC response is the length of the KCM response (4
    # bytes), a status code which is essentially always 0 (4 bytes),
    # and the KCM response.
    kcm_response = struct.pack('>l', code) + payload
    hipc_response = struct.pack('>LL', len(kcm_response), 0) + kcm_response
    s.sendall(hipc_response)
    return True

parser = optparse.OptionParser()
parser.add_option('-f', '--fallback', action='store_true', dest='fallback',
                  default=False,
                  help='Do not support RETRIEVE/GET_CRED_LIST/REPLACE')
(options, args) = parser.parse_args()
if options.fallback:
    del ophandlers[KCMOpcodes.RETRIEVE]
    del ophandlers[KCMOpcodes.GET_CRED_LIST]
    del ophandlers[KCMOpcodes.REPLACE]

server = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
server.bind(args[0])
server.listen(5)
select_input = [server,]
sys.stderr.write('starting...\n')
sys.stderr.flush()

while True:
    iready, oready, xready = select.select(select_input, [], [])
    for s in iready:
        if s == server:
            client, addr = server.accept()
            select_input.append(client)
        else:
            if not service_request(s):
                select_input.remove(s)
                s.close()
