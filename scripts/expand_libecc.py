#/*
# *  Copyright (C) 2017 - This file is part of libecc project
# *
# *  Authors:
# *      Ryad BENADJILA <ryadbenadjila@gmail.com>
# *      Arnaud EBALARD <arnaud.ebalard@ssi.gouv.fr>
# *      Jean-Pierre FLORI <jean-pierre.flori@ssi.gouv.fr>
# *
# *  Contributors:
# *      Nicolas VIVET <nicolas.vivet@ssi.gouv.fr>
# *      Karim KHALFALLAH <karim.khalfallah@ssi.gouv.fr>
# *
# *  This software is licensed under a dual BSD and GPL v2 license.
# *  See LICENSE file at the root folder of the project.
# */
#! /usr/bin/env python

import random, sys, re, math, os, getopt, glob, copy, hashlib, binascii, string, signal, base64

# External dependecy for SHA-3
# It is an independent module, since hashlib has no support
# for SHA-3 functions for now
import sha3

# Handle Python 2/3 issues
def is_python_2():
    if sys.version_info[0] < 3:
        return True
    else:
        return False

### Ctrl-C handler
def handler(signal, frame):
    print("\nSIGINT caught: exiting ...")
    exit(0)

# Helper to ask the user for something
def get_user_input(prompt):
    # Handle the Python 2/3 issue
    if is_python_2() == False:
        return input(prompt)
    else:
        return raw_input(prompt)

##########################################################
#### Math helpers
def egcd(b, n):
    x0, x1, y0, y1 = 1, 0, 0, 1
    while n != 0:
        q, b, n = b // n, n, b % n
        x0, x1 = x1, x0 - q * x1
        y0, y1 = y1, y0 - q * y1
    return  b, x0, y0

def modinv(a, m):
    g, x, y = egcd(a, m)
    if g != 1:
        raise Exception("Error: modular inverse does not exist")
    else:
        return x % m

def compute_monty_coef(prime, pbitlen, wlen):
    """
    Compute montgomery coeff r, r^2 and mpinv. pbitlen is the size
    of p in bits. It is expected to be a multiple of word
    bit size.
    """
    r = (1 << int(pbitlen)) % prime
    r_square = (1 << (2 * int(pbitlen))) % prime
    mpinv = 2**wlen - (modinv(prime, 2**wlen))
    return r, r_square, mpinv

def compute_div_coef(prime, pbitlen, wlen):
    """
    Compute division coeffs p_normalized, p_shift and p_reciprocal.
    """
    tmp = prime
    cnt = 0
    while tmp != 0:
        tmp = tmp >> 1
        cnt += 1
    pshift = int(pbitlen - cnt)
    primenorm = prime << pshift
    B = 2**wlen
    prec = B**3 // ((primenorm >> int(pbitlen - 2*wlen)) + 1) - B
    return pshift, primenorm, prec

def is_probprime(n):
    # ensure n is odd
    if n % 2 == 0:
        return False
    # write n-1 as 2**s * d
    # repeatedly try to divide n-1 by 2
    s = 0
    d = n-1
    while True:
        quotient, remainder = divmod(d, 2)
        if remainder == 1:
            break
        s += 1
        d = quotient
    assert(2**s * d == n-1)
    # test the base a to see whether it is a witness for the compositeness of n
    def try_composite(a):
        if pow(a, d, n) == 1:
            return False
        for i in range(s):
            if pow(a, 2**i * d, n) == n-1:
                return False
        return True # n is definitely composite
    for i in range(5):
        a = random.randrange(2, n)
        if try_composite(a):
            return False
    return True # no base tested showed n as composite

def legendre_symbol(a, p):
    ls = pow(a, (p - 1) // 2, p)
    return -1 if ls == p - 1 else ls

# Tonelli-Shanks algorithm to find square roots
# over prime fields
def mod_sqrt(a, p):
    # Square root of 0 is 0
    if a == 0:
        return 0
    # Simple cases
    if legendre_symbol(a, p) != 1:
        # No square residue
        return None
    elif p == 2:
        return a
    elif p % 4 == 3:
        return pow(a, (p + 1) // 4, p)
    s = p - 1
    e = 0
    while s % 2 == 0:
        s = s // 2
        e += 1
    n = 2
    while legendre_symbol(n, p) != -1:
        n += 1
    x = pow(a, (s + 1) // 2, p)
    b = pow(a, s, p)
    g = pow(n, s, p)
    r = e
    while True:
        t = b
        m = 0
        if is_python_2():
            for m in xrange(r):
                if t == 1:
                    break
                t = pow(t, 2, p)
        else:
            for m in range(r):
                if t == 1:
                    break
                t = pow(t, 2, p)
        if m == 0:
            return x
        gs = pow(g, 2 ** (r - m - 1), p)
        g = (gs * gs) % p
        x = (x * gs) % p
        b = (b * g) % p
        r = m

##########################################################
### Math elliptic curves basic blocks

# WARNING: these blocks are only here for testing purpose and
# are not intended to be used in a security oriented library!
# This explains the usage of naive affine coordinates fomulas
class Curve(object):
    def __init__(self, a, b, prime, order, cofactor, gx, gy, npoints, name, oid):
        self.a = a
        self.b = b
        self.p = prime
        self.q = order
        self.c = cofactor
        self.gx = gx
        self.gy = gy
        self.n = npoints
        self.name = name
        self.oid = oid
    # Equality testing
    def __eq__(self, other):
        return self.__dict__ == other.__dict__
    # Deep copy is implemented using the ~X operator
    def __invert__(self):
        return copy.deepcopy(self)


class Point(object):
    # Affine coordinates (x, y), infinity point is (None, None)
    def __init__(self, curve, x, y):
        self.curve = curve
        if x != None:
            self.x = (x % curve.p)
        else:
            self.x = None
        if y != None:
            self.y = (y % curve.p)
        else:
            self.y = None
        # Check that the point is indeed on the curve
        if (x != None):
            if (pow(y, 2, curve.p) != ((pow(x, 3, curve.p) + (curve.a * x) + curve.b ) % curve.p)):
                raise Exception("Error: point is not on curve!")
    # Addition
    def __add__(self, Q):
        x1 = self.x
        y1 = self.y
        x2 = Q.x
        y2 = Q.y
        curve = self.curve
        # Check that we are on the same curve
        if Q.curve != curve:
            raise Exception("Point add error: two point don't have the same curve")
        # If Q is infinity point, return ourself
        if Q.x == None:
            return Point(self.curve, self.x, self.y)
        # If we are the infinity point return Q
        if self.x == None:
            return Q
        # Infinity point or Doubling
        if (x1 == x2):
            if (((y1 + y2) % curve.p) == 0):
                # Return infinity point
                return Point(self.curve, None, None)
            else:
                # Doubling
                L = ((3*pow(x1, 2, curve.p) + curve.a) * modinv(2*y1, curve.p)) % curve.p
        # Addition
        else:
            L = ((y2 - y1) * modinv((x2 - x1) % curve.p, curve.p)) % curve.p
        resx = (pow(L, 2, curve.p) - x1 - x2) % curve.p
        resy = ((L * (x1 - resx)) - y1) % curve.p
        # Return the point
        return Point(self.curve, resx, resy)
    # Negation
    def __neg__(self):
        if (self.x == None):
            return Point(self.curve, None, None)
        else:
            return Point(self.curve, self.x, -self.y)
    # Subtraction
    def __sub__(self, other):
        return self + (-other)
    # Scalar mul
    def __rmul__(self, scalar):
        # Implement simple double and add algorithm
        P = self
        Q = Point(P.curve, None, None)
        for i in range(getbitlen(scalar), 0, -1):
            Q = Q + Q
            if (scalar >> (i-1)) & 0x1 == 0x1:
                Q = Q + P
        return Q
    # Equality testing
    def __eq__(self, other):
        return self.__dict__ == other.__dict__
    # Deep copy is implemented using the ~X operator
    def __invert__(self):
        return copy.deepcopy(self)
    def __str__(self):
        if self.x == None:
            return "Inf"
        else:
            return ("(x = %s, y = %s)" % (hex(self.x), hex(self.y)))

##########################################################
### Private and public keys structures
class PrivKey(object):
    def __init__(self, curve, x):
        self.curve = curve
        self.x = x

class PubKey(object):
    def __init__(self, curve, Y):
        # Sanity check
        if Y.curve != curve:
            raise Exception("Error: curve and point curve differ in public key!")
        self.curve = curve
        self.Y = Y

class KeyPair(object):
    def __init__(self, pubkey, privkey):
        self.pubkey = pubkey
        self.privkey = privkey


def fromprivkey(privkey, is_eckcdsa=False):
    curve = privkey.curve
    q = curve.q
    gx = curve.gx
    gy = curve.gy
    G = Point(curve, gx, gy)
    if is_eckcdsa == False:
        return PubKey(curve, privkey.x * G)
    else:
        return PubKey(curve, modinv(privkey.x, q) * G)

def genKeyPair(curve, is_eckcdsa=False):
    p = curve.p
    q = curve.q
    gx = curve.gx
    gy = curve.gy
    G = Point(curve, gx, gy)
    OK = False
    while OK == False:
        x = getrandomint(q)
        if x == 0:
            continue
        OK = True
    privkey = PrivKey(curve, x)
    pubkey = fromprivkey(privkey, is_eckcdsa)
    return KeyPair(pubkey, privkey)

##########################################################
### Signature algorithms helpers
def getrandomint(modulo):
    return random.randrange(0, modulo+1)

def getbitlen(bint):
    """
    Returns the number of bits encoding an integer
    """
    if bint == None:
        return 0
    if bint == 0:
        # Zero is encoded on one bit
        return 1
    else:
        return int(bint).bit_length()

def getbytelen(bint):
    """
    Returns the number of bytes encoding an integer
    """
    bitsize = getbitlen(bint)
    bytesize = int(bitsize // 8)
    if bitsize % 8 != 0:
        bytesize += 1
    return bytesize

def stringtoint(bitstring):
    acc = 0
    size = len(bitstring)
    for i in range(0, size):
        acc = acc + (ord(bitstring[i]) * (2**(8*(size - 1 - i))))
    return acc

def inttostring(a):
    size = int(getbytelen(a))
    outstr = ""
    for i in range(0, size):
        outstr = outstr + chr((a >> (8*(size - 1 - i))) & 0xFF)
    return outstr

def expand(bitstring, bitlen, direction):
    bytelen = int(math.ceil(bitlen / 8.))
    if len(bitstring) >= bytelen:
        return bitstring
    else:
        if direction == "LEFT":
            return ((bytelen-len(bitstring))*"\x00") + bitstring
        elif direction == "RIGHT":
            return bitstring + ((bytelen-len(bitstring))*"\x00")
        else:
            raise Exception("Error: unknown direction "+direction+" in expand")

def truncate(bitstring, bitlen, keep):
    """
    Takes a bit string and truncates it to keep the left
    most or the right most bits
    """
    strbitlen = 8*len(bitstring)
    # Check if truncation is needed
    if strbitlen > bitlen:
        if keep == "LEFT":
            return expand(inttostring(stringtoint(bitstring) >> int(strbitlen - bitlen)), bitlen, "LEFT")
        elif keep == "RIGHT":
            mask = (2**bitlen)-1
            return expand(inttostring(stringtoint(bitstring) & mask), bitlen, "LEFT")
        else:
            raise Exception("Error: unknown direction "+keep+" in truncate")
    else:
        # No need to truncate!
        return bitstring

##########################################################
### Hash algorithms
def sha224(message):
    ctx = hashlib.sha224()
    if(is_python_2() == True):
        ctx.update(message)
        digest = ctx.digest()
    else:
        ctx.update(message.encode('latin-1'))
        digest = ctx.digest().decode('latin-1')
    return (digest, ctx.digest_size, ctx.block_size)

def sha256(message):
    ctx = hashlib.sha256()
    if(is_python_2() == True):
        ctx.update(message)
        digest = ctx.digest()
    else:
        ctx.update(message.encode('latin-1'))
        digest = ctx.digest().decode('latin-1')
    return (digest, ctx.digest_size, ctx.block_size)

def sha384(message):
    ctx = hashlib.sha384()
    if(is_python_2() == True):
        ctx.update(message)
        digest = ctx.digest()
    else:
        ctx.update(message.encode('latin-1'))
        digest = ctx.digest().decode('latin-1')
    return (digest, ctx.digest_size, ctx.block_size)

def sha512(message):
    ctx = hashlib.sha512()
    if(is_python_2() == True):
        ctx.update(message)
        digest = ctx.digest()
    else:
        ctx.update(message.encode('latin-1'))
        digest = ctx.digest().decode('latin-1')
    return (digest, ctx.digest_size, ctx.block_size)

def sha3_224(message):
    ctx = sha3.Sha3_ctx(224)
    if(is_python_2() == True):
        ctx.update(message)
        digest = ctx.digest()
    else:
        ctx.update(message.encode('latin-1'))
        digest = ctx.digest().decode('latin-1')
    return (digest, ctx.digest_size, ctx.block_size)

def sha3_256(message):
    ctx = sha3.Sha3_ctx(256)
    if(is_python_2() == True):
        ctx.update(message)
        digest = ctx.digest()
    else:
        ctx.update(message.encode('latin-1'))
        digest = ctx.digest().decode('latin-1')
    return (digest, ctx.digest_size, ctx.block_size)

def sha3_384(message):
    ctx = sha3.Sha3_ctx(384)
    if(is_python_2() == True):
        ctx.update(message)
        digest = ctx.digest()
    else:
        ctx.update(message.encode('latin-1'))
        digest = ctx.digest().decode('latin-1')
    return (digest, ctx.digest_size, ctx.block_size)

def sha3_512(message):
    ctx = sha3.Sha3_ctx(512)
    if(is_python_2() == True):
        ctx.update(message)
        digest = ctx.digest()
    else:
        ctx.update(message.encode('latin-1'))
        digest = ctx.digest().decode('latin-1')
    return (digest, ctx.digest_size, ctx.block_size)

##########################################################
### Signature algorithms

# *| IUF  - ECDSA signature
# *|
# *|  UF  1. Compute h = H(m)
# *|   F  2. If |h| > bitlen(q), set h to bitlen(q)
# *|         leftmost (most significant) bits of h
# *|   F  3. e = OS2I(h) mod q
# *|   F  4. Get a random value k in ]0,q[
# *|   F  5. Compute W = (W_x,W_y) = kG
# *|   F  6. Compute r = W_x mod q
# *|   F  7. If r is 0, restart the process at step 4.
# *|   F  8. If e == rx, restart the process at step 4.
# *|   F  9. Compute s = k^-1 * (xr + e) mod q
# *|   F 10. If s is 0, restart the process at step 4.
# *|   F 11. Return (r,s)
def ecdsa_sign(hashfunc, keypair, message, k=None):
    privkey = keypair.privkey
    # Get important parameters from the curve
    p = privkey.curve.p
    q = privkey.curve.q
    gx = privkey.curve.gx
    gy = privkey.curve.gy
    G = Point(privkey.curve, gx, gy)
    q_limit_len = getbitlen(q)
    # Compute the hash
    (h, _, _) = hashfunc(message)
    # Truncate hash value
    h = truncate(h, q_limit_len, "LEFT")
    # Convert the hash value to an int
    e = stringtoint(h) % q
    OK = False
    while OK == False:
        if k == None:
            k = getrandomint(q)
        if k == 0:
            continue
        W = k * G
        r = W.x % q
        if r == 0:
            continue
        if e == r * privkey.x:
            continue
        s = (modinv(k, q) * ((privkey.x * r) + e)) % q
        if s == 0:
            continue
        OK = True
    return ((expand(inttostring(r), 8*getbytelen(q), "LEFT") + expand(inttostring(s), 8*getbytelen(q), "LEFT")), k)

# *| IUF  - ECDSA verification
# *|
# *| I    1. Reject the signature if r or s is 0.
# *|  UF  2. Compute h = H(m)
# *|   F  3. If |h| > bitlen(q), set h to bitlen(q)
# *|         leftmost (most significant) bits of h
# *|   F  4. Compute e = OS2I(h) mod q
# *|   F  5. Compute u = (s^-1)e mod q
# *|   F  6. Compute v = (s^-1)r mod q
# *|   F  7. Compute W' = uG + vY
# *|   F  8. If W' is the point at infinity, reject the signature.
# *|   F  9. Compute r' = W'_x mod q
# *|   F 10. Accept the signature if and only if r equals r'
def ecdsa_verify(hashfunc, keypair, message, sig):
    pubkey = keypair.pubkey
    # Get important parameters from the curve
    p = pubkey.curve.p
    q = pubkey.curve.q
    gx = pubkey.curve.gx
    gy = pubkey.curve.gy
    q_limit_len = getbitlen(q)
    G = Point(pubkey.curve, gx, gy)
    # Extract r and s
    if len(sig) != 2*getbytelen(q):
        raise Exception("ECDSA verify: bad signature length!")
    r = stringtoint(sig[0:int(len(sig)/2)])
    s = stringtoint(sig[int(len(sig)/2):])
    if r == 0 or s == 0:
        return False
    # Compute the hash
    (h, _, _) = hashfunc(message)
    # Truncate hash value
    h = truncate(h, q_limit_len, "LEFT")
    # Convert the hash value to an int
    e = stringtoint(h) % q
    u = (modinv(s, q) * e) % q
    v = (modinv(s, q) * r) % q
    W_ = (u * G) + (v * pubkey.Y)
    if W_.x == None:
        return False
    r_ = W_.x % q
    if r == r_:
        return True
    else:
        return False

def eckcdsa_genKeyPair(curve):
    return genKeyPair(curve, True)

# *| IUF  - ECKCDSA signature
# *|
# *| IUF  1. Compute h = H(z||m)
# *|   F  2. If hsize > bitlen(q), set h to bitlen(q)
# *|         rightmost (less significant) bits of h.
# *|   F  3. Get a random value k in ]0,q[
# *|   F  4. Compute W = (W_x,W_y) = kG
# *|   F  5. Compute r = h(FE2OS(W_x)).
# *|   F  6. If hsize > bitlen(q), set r to bitlen(q)
# *|         rightmost (less significant) bits of r.
# *|   F  7. Compute e = OS2I(r XOR h) mod q
# *|   F  8. Compute s = x(k - e) mod q
# *|   F  9. if s == 0, restart at step 3.
# *|   F 10. return (r,s)
def eckcdsa_sign(hashfunc, keypair, message, k=None):
    privkey = keypair.privkey
    # Get important parameters from the curve
    p = privkey.curve.p
    q = privkey.curve.q
    gx = privkey.curve.gx
    gy = privkey.curve.gy
    G = Point(privkey.curve, gx, gy)
    q_limit_len = getbitlen(q)
    # Compute the certificate data
    (_, _, hblocksize) = hashfunc("")
    z = expand(inttostring(keypair.pubkey.Y.x), 8*getbytelen(p), "LEFT")
    z = z + expand(inttostring(keypair.pubkey.Y.y), 8*getbytelen(p), "LEFT")
    if len(z) > hblocksize:
        # Truncate
        z = truncate(z, 8*hblocksize, "LEFT")
    else:
        # Expand
        z = expand(z, 8*hblocksize, "RIGHT")
    # Compute the hash
    (h, _, _) = hashfunc(z + message)
    # Truncate hash value
    h = truncate(h, 8 * int(math.ceil(q_limit_len / 8)), "RIGHT")
    OK = False
    while OK == False:
        if k == None:
            k = getrandomint(q)
        if k == 0:
            continue
        W = k * G
        (r, _, _) = hashfunc(expand(inttostring(W.x), 8*getbytelen(p), "LEFT"))
        r = truncate(r, 8 * int(math.ceil(q_limit_len / 8)), "RIGHT")
        e = (stringtoint(r) ^ stringtoint(h)) % q
        s = (privkey.x * (k - e)) % q
        if s == 0:
            continue
        OK = True
    return (r + expand(inttostring(s), 8*getbytelen(q), "LEFT"), k)

# *| IUF - ECKCDSA verification
# *|
# *| I   1. Check the length of r:
# *|         - if hsize > bitlen(q), r must be of
# *|           length bitlen(q)
# *|         - if hsize <= bitlen(q), r must be of
# *|           length hsize
# *| I   2. Check that s is in ]0,q[
# *| IUF 3. Compute h = H(z||m)
# *|   F 4. If hsize > bitlen(q), set h to bitlen(q)
# *|        rightmost (less significant) bits of h.
# *|   F 5. Compute e = OS2I(r XOR h) mod q
# *|   F 6. Compute W' = sY + eG, where Y is the public key
# *|   F 7. Compute r' = h(FE2OS(W'x))
# *|   F 8. If hsize > bitlen(q), set r' to bitlen(q)
# *|        rightmost (less significant) bits of r'.
# *|   F 9. Check if r == r'
def eckcdsa_verify(hashfunc, keypair, message, sig):
    pubkey = keypair.pubkey
    # Get important parameters from the curve
    p = pubkey.curve.p
    q = pubkey.curve.q
    gx = pubkey.curve.gx
    gy = pubkey.curve.gy
    G = Point(pubkey.curve, gx, gy)
    q_limit_len = getbitlen(q)
    (_, hsize, hblocksize) = hashfunc("")
    # Extract r and s
    if (8*hsize) > q_limit_len:
        r_len = int(math.ceil(q_limit_len / 8.))
    else:
        r_len = hsize
    r = stringtoint(sig[0:int(r_len)])
    s = stringtoint(sig[int(r_len):])
    if (s >= q) or (s < 0):
        return False
    # Compute the certificate data
    z = expand(inttostring(keypair.pubkey.Y.x), 8*getbytelen(p), "LEFT")
    z = z + expand(inttostring(keypair.pubkey.Y.y), 8*getbytelen(p), "LEFT")
    if len(z) > hblocksize:
        # Truncate
        z = truncate(z, 8*hblocksize, "LEFT")
    else:
        # Expand
        z = expand(z, 8*hblocksize, "RIGHT")
    # Compute the hash
    (h, _, _) = hashfunc(z + message)
    # Truncate hash value
    h = truncate(h, 8 * int(math.ceil(q_limit_len / 8)), "RIGHT")
    e = (r ^ stringtoint(h)) % q
    W_ = (s * pubkey.Y) + (e * G)
    (h, _, _) = hashfunc(expand(inttostring(W_.x), 8*getbytelen(p), "LEFT"))
    r_ = truncate(h, 8 * int(math.ceil(q_limit_len / 8)), "RIGHT")
    if stringtoint(r_) == r:
        return True
    else:
        return False

# *| IUF - ECFSDSA signature
# *|
# *| I   1. Get a random value k in ]0,q[
# *| I   2. Compute W = (W_x,W_y) = kG
# *| I   3. Compute r = FE2OS(W_x)||FE2OS(W_y)
# *| I   4. If r is an all zero string, restart the process at step 1.
# *| IUF 5. Compute h = H(r||m)
# *|   F 6. Compute e = OS2I(h) mod q
# *|   F 7. Compute s = (k + ex) mod q
# *|   F 8. If s is 0, restart the process at step 1 (see c. below)
# *|   F 9. Return (r,s)
def ecfsdsa_sign(hashfunc, keypair, message, k=None):
    privkey = keypair.privkey
    # Get important parameters from the curve
    p = privkey.curve.p
    q = privkey.curve.q
    gx = privkey.curve.gx
    gy = privkey.curve.gy
    G = Point(privkey.curve, gx, gy)
    OK = False
    while OK == False:
        if k == None:
            k = getrandomint(q)
        if k == 0:
            continue
        W = k * G
        r = expand(inttostring(W.x), 8*getbytelen(p), "LEFT") + expand(inttostring(W.y), 8*getbytelen(p), "LEFT")
        if stringtoint(r) == 0:
            continue
        (h, _, _) = hashfunc(r + message)
        e = stringtoint(h) % q
        s = (k + e * privkey.x) % q
        if s == 0:
            continue
        OK = True
    return (r + expand(inttostring(s), 8*getbytelen(q), "LEFT"), k)


# *| IUF - ECFSDSA verification
# *|
# *| I   1. Reject the signature if r is not a valid point on the curve.
# *| I   2. Reject the signature if s is not in ]0,q[
# *| IUF 3. Compute h = H(r||m)
# *|   F 4. Convert h to an integer and then compute e = -h mod q
# *|   F 5. compute W' = sG + eY, where Y is the public key
# *|   F 6. Compute r' = FE2OS(W'_x)||FE2OS(W'_y)
# *|   F 7. Accept the signature if and only if r equals r'
def ecfsdsa_verify(hashfunc, keypair, message, sig):
    pubkey = keypair.pubkey
    # Get important parameters from the curve
    p = pubkey.curve.p
    q = pubkey.curve.q
    gx = pubkey.curve.gx
    gy = pubkey.curve.gy
    G = Point(pubkey.curve, gx, gy)
    # Extract coordinates from r and s from signature
    if len(sig) != (2*getbytelen(p)) + getbytelen(q):
        raise Exception("ECFSDSA verify: bad signature length!")
    wx = sig[:int(getbytelen(p))]
    wy = sig[int(getbytelen(p)):int(2*getbytelen(p))]
    r = wx + wy
    s = stringtoint(sig[int(2*getbytelen(p)):int((2*getbytelen(p))+getbytelen(q))])
    # Check r is on the curve
    W = Point(pubkey.curve, stringtoint(wx), stringtoint(wy))
    # Check s is in ]0,q[
    if s == 0 or s > q:
        raise Exception("ECFSDSA verify: s not in ]0,q[")
    (h, _, _) = hashfunc(r + message)
    e = (-stringtoint(h)) % q
    W_ = s * G + e * pubkey.Y
    r_ = expand(inttostring(W_.x), 8*getbytelen(p), "LEFT") + expand(inttostring(W_.y), 8*getbytelen(p), "LEFT")
    if r == r_:
        return True
    else:
        return False


# NOTE: ISO/IEC 14888-3 standard seems to diverge from the existing implementations
# of ECRDSA when treating the message hash, and from the examples of certificates provided
# in RFC 7091 and draft-deremin-rfc4491-bis. While in ISO/IEC 14888-3 it is explicitely asked
# to proceed with the hash of the message as big endian, the RFCs derived from the Russian
# standard expect the hash value to be treated as little endian when importing it as an integer
# (this discrepancy is exhibited and confirmed by test vectors present in ISO/IEC 14888-3, and
# by X.509 certificates present in the RFCs). This seems (to be confirmed) to be a discrepancy of
# ISO/IEC 14888-3 algorithm description that must be fixed there.
#
# In order to be conservative, libecc uses the Russian standard behavior as expected to be in line with
# other implemetations, but keeps the ISO/IEC 14888-3 behavior if forced/asked by the user using
# the USE_ISO14888_3_ECRDSA toggle. This allows to keep backward compatibility with previous versions of the
# library if needed.

# *| IUF - ECRDSA signature
# *|
# *|  UF  1. Compute h = H(m)
# *|   F  2. Get a random value k in ]0,q[
# *|   F  3. Compute W = (W_x,W_y) = kG
# *|   F  4. Compute r = W_x mod q
# *|   F  5. If r is 0, restart the process at step 2.
# *|   F  6. Compute e = OS2I(h) mod q. If e is 0, set e to 1.
# *|         NOTE: here, ISO/IEC 14888-3 and RFCs differ in the way e treated.
# *|         e = OS2I(h) for ISO/IEC 14888-3, or e = OS2I(reversed(h)) when endianness of h
# *|         is reversed for RFCs.
# *|   F  7. Compute s = (rx + ke) mod q
# *|   F  8. If s is 0, restart the process at step 2.
# *|   F 11. Return (r,s)
def ecrdsa_sign(hashfunc, keypair, message, k=None, use_iso14888_divergence=False):
    privkey = keypair.privkey
    # Get important parameters from the curve
    p = privkey.curve.p
    q = privkey.curve.q
    gx = privkey.curve.gx
    gy = privkey.curve.gy
    G = Point(privkey.curve, gx, gy)
    (h, _, _) = hashfunc(message)
    if use_iso14888_divergence == False:
        # Reverse the endianness for Russian standard RFC ECRDSA (contrary to ISO/IEC 14888-3 case)
        h = h[::-1]
    OK = False
    while OK == False:
        if k == None:
            k = getrandomint(q)
        if k == 0:
            continue
        W = k * G
        r = W.x % q
        if r == 0:
            continue
        e = stringtoint(h) % q
        if e == 0:
            e = 1
        s = ((r * privkey.x) + (k * e)) % q
        if s == 0:
            continue
        OK = True
    return (expand(inttostring(r), 8*getbytelen(q), "LEFT") + expand(inttostring(s), 8*getbytelen(q), "LEFT"), k)

# *| IUF - ECRDSA verification
# *|
# *|  UF 1. Check that r and s are both in ]0,q[
# *|   F 2. Compute h = H(m)
# *|   F 3. Compute e = OS2I(h)^-1 mod q
# *|         NOTE: here, ISO/IEC 14888-3 and RFCs differ in the way e treated.
# *|         e = OS2I(h) for ISO/IEC 14888-3, or e = OS2I(reversed(h)) when endianness of h
# *|         is reversed for RFCs.
# *|   F 4. Compute u = es mod q
# *|   F 4. Compute v = -er mod q
# *|   F 5. Compute W' = uG + vY = (W'_x, W'_y)
# *|   F 6. Let's now compute r' = W'_x mod q
# *|   F 7. Check r and r' are the same
def ecrdsa_verify(hashfunc, keypair, message, sig, use_iso14888_divergence=False):
    pubkey = keypair.pubkey
    # Get important parameters from the curve
    p = pubkey.curve.p
    q = pubkey.curve.q
    gx = pubkey.curve.gx
    gy = pubkey.curve.gy
    G = Point(pubkey.curve, gx, gy)
    # Extract coordinates from r and s from signature
    if len(sig) != 2*getbytelen(q):
        raise Exception("ECRDSA verify: bad signature length!")
    r = stringtoint(sig[:int(getbytelen(q))])
    s = stringtoint(sig[int(getbytelen(q)):int(2*getbytelen(q))])
    if r == 0 or r > q:
        raise Exception("ECRDSA verify: r not in ]0,q[")
    if s == 0 or s > q:
        raise Exception("ECRDSA verify: s not in ]0,q[")
    (h, _, _) = hashfunc(message)
    if use_iso14888_divergence == False:
        # Reverse the endianness for Russian standard RFC ECRDSA (contrary to ISO/IEC 14888-3 case)
        h = h[::-1]
    e = modinv(stringtoint(h) % q, q)
    u = (e * s) % q
    v = (-e * r) % q
    W_ = u * G + v * pubkey.Y
    r_ = W_.x % q
    if r == r_:
        return True
    else:
        return False


# *| IUF - ECGDSA signature
# *|
# *|  UF 1. Compute h = H(m). If |h| > bitlen(q), set h to bitlen(q)
# *|         leftmost (most significant) bits of h
# *|   F 2. Convert e = - OS2I(h) mod q
# *|   F 3. Get a random value k in ]0,q[
# *|   F 4. Compute W = (W_x,W_y) = kG
# *|   F 5. Compute r = W_x mod q
# *|   F 6. If r is 0, restart the process at step 4.
# *|   F 7. Compute s = x(kr + e) mod q
# *|   F 8. If s is 0, restart the process at step 4.
# *|   F 9. Return (r,s)
def ecgdsa_sign(hashfunc, keypair, message, k=None):
    privkey = keypair.privkey
    # Get important parameters from the curve
    p = privkey.curve.p
    q = privkey.curve.q
    gx = privkey.curve.gx
    gy = privkey.curve.gy
    G = Point(privkey.curve, gx, gy)
    (h, _, _) = hashfunc(message)
    q_limit_len = getbitlen(q)
    # Truncate hash value
    h = truncate(h, q_limit_len, "LEFT")
    e = (-stringtoint(h)) % q
    OK = False
    while OK == False:
        if k == None:
            k = getrandomint(q)
        if k == 0:
            continue
        W = k * G
        r = W.x % q
        if r == 0:
            continue
        s = (privkey.x * ((k * r) + e)) % q
        if s == 0:
            continue
        OK = True
    return (expand(inttostring(r), 8*getbytelen(q), "LEFT") + expand(inttostring(s), 8*getbytelen(q), "LEFT"), k)

# *| IUF - ECGDSA verification
# *|
# *| I   1. Reject the signature if r or s is 0.
# *|  UF 2. Compute h = H(m). If |h| > bitlen(q), set h to bitlen(q)
# *|         leftmost (most significant) bits of h
# *|   F 3. Compute e = OS2I(h) mod q
# *|   F 4. Compute u = ((r^-1)e mod q)
# *|   F 5. Compute v = ((r^-1)s mod q)
# *|   F 6. Compute W' = uG + vY
# *|   F 7. Compute r' = W'_x mod q
# *|   F 8. Accept the signature if and only if r equals r'
def ecgdsa_verify(hashfunc, keypair, message, sig):
    pubkey = keypair.pubkey
    # Get important parameters from the curve
    p = pubkey.curve.p
    q = pubkey.curve.q
    gx = pubkey.curve.gx
    gy = pubkey.curve.gy
    G = Point(pubkey.curve, gx, gy)
    # Extract coordinates from r and s from signature
    if len(sig) != 2*getbytelen(q):
        raise Exception("ECGDSA verify: bad signature length!")
    r = stringtoint(sig[:int(getbytelen(q))])
    s = stringtoint(sig[int(getbytelen(q)):int(2*getbytelen(q))])
    if r == 0 or r > q:
        raise Exception("ECGDSA verify: r not in ]0,q[")
    if s == 0 or s > q:
        raise Exception("ECGDSA verify: s not in ]0,q[")
    (h, _, _) = hashfunc(message)
    q_limit_len = getbitlen(q)
    # Truncate hash value
    h = truncate(h, q_limit_len, "LEFT")
    e = stringtoint(h) % q
    r_inv = modinv(r, q)
    u = (r_inv * e) % q
    v = (r_inv * s) % q
    W_ = u * G + v * pubkey.Y
    r_ = W_.x % q
    if r == r_:
        return True
    else:
        return False

# *| IUF - ECSDSA/ECOSDSA signature
# *|
# *| I   1. Get a random value k in ]0, q[
# *| I   2. Compute W = kG = (Wx, Wy)
# *| IUF 3. Compute r = H(Wx [|| Wy] || m)
# *|        - In the normal version (ECSDSA), r = h(Wx || Wy || m).
# *|        - In the optimized version (ECOSDSA), r = h(Wx || m).
# *|   F 4. Compute e = OS2I(r) mod q
# *|   F 5. if e == 0, restart at step 1.
# *|   F 6. Compute s = (k + ex) mod q.
# *|   F 7. if s == 0, restart at step 1.
# *|   F 8. Return (r, s)
def ecsdsa_common_sign(hashfunc, keypair, message, optimized, k=None):
    privkey = keypair.privkey
    # Get important parameters from the curve
    p = privkey.curve.p
    q = privkey.curve.q
    gx = privkey.curve.gx
    gy = privkey.curve.gy
    G = Point(privkey.curve, gx, gy)
    OK = False
    while OK == False:
        if k == None:
            k = getrandomint(q)
        if k == 0:
            continue
        W = k * G
        if optimized == False:
            (r, _, _) = hashfunc(expand(inttostring(W.x), 8*getbytelen(p), "LEFT") + expand(inttostring(W.y), 8*getbytelen(p), "LEFT") + message)
        else:
            (r, _, _) = hashfunc(expand(inttostring(W.x), 8*getbytelen(p), "LEFT") + message)
        e = stringtoint(r) % q
        if e == 0:
            continue
        s = (k + (e * privkey.x)) % q
        if s == 0:
            continue
        OK = True
    return (r + expand(inttostring(s), 8*getbytelen(q), "LEFT"), k)

def ecsdsa_sign(hashfunc, keypair, message, k=None):
    return ecsdsa_common_sign(hashfunc, keypair, message, False, k)

def ecosdsa_sign(hashfunc, keypair, message, k=None):
    return ecsdsa_common_sign(hashfunc, keypair, message, True, k)

# *| IUF - ECSDSA/ECOSDSA verification
# *|
# *| I   1. if s is not in ]0,q[, reject the signature.x
# *| I   2. Compute e = -r mod q
# *| I   3. If e == 0, reject the signature.
# *| I   4. Compute W' = sG + eY
# *| IUF 5. Compute r' = H(W'x [|| W'y] || m)
# *|        - In the normal version (ECSDSA), r = h(W'x || W'y || m).
# *|        - In the optimized version (ECOSDSA), r = h(W'x || m).
# *|   F 6. Accept the signature if and only if r and r' are the same
def ecsdsa_common_verify(hashfunc, keypair, message, sig, optimized):
    pubkey = keypair.pubkey
    # Get important parameters from the curve
    p = pubkey.curve.p
    q = pubkey.curve.q
    gx = pubkey.curve.gx
    gy = pubkey.curve.gy
    G = Point(pubkey.curve, gx, gy)
    (_, hlen, _) = hashfunc("")
    # Extract coordinates from r and s from signature
    if len(sig) != hlen + getbytelen(q):
        raise Exception("EC[O]SDSA verify: bad signature length!")
    r = stringtoint(sig[:int(hlen)])
    s = stringtoint(sig[int(hlen):int(hlen+getbytelen(q))])
    if s == 0 or s > q:
        raise Exception("EC[O]DSA verify: s not in ]0,q[")
    e = (-r) % q
    if e == 0:
        raise Exception("EC[O]DSA verify: e is null")
    W_ = s * G + e * pubkey.Y
    if optimized == False:
        (r_, _, _) = hashfunc(expand(inttostring(W_.x), 8*getbytelen(p), "LEFT") + expand(inttostring(W_.y), 8*getbytelen(p), "LEFT") + message)
    else:
        (r_, _, _) = hashfunc(expand(inttostring(W_.x), 8*getbytelen(p), "LEFT") + message)
    if sig[:int(hlen)] == r_:
        return True
    else:
        return False

def ecsdsa_verify(hashfunc, keypair, message, sig):
    return ecsdsa_common_verify(hashfunc, keypair, message, sig, False)

def ecosdsa_verify(hashfunc, keypair, message, sig):
    return ecsdsa_common_verify(hashfunc, keypair, message, sig, True)


##########################################################
### Generate self-tests for all the algorithms

all_hash_funcs = [ (sha224, "SHA224"), (sha256, "SHA256"), (sha384, "SHA384"), (sha512, "SHA512"), (sha3_224, "SHA3_224"), (sha3_256, "SHA3_256"), (sha3_384, "SHA3_384"), (sha3_512, "SHA3_512") ]

all_sig_algs = [ (ecdsa_sign, ecdsa_verify, genKeyPair, "ECDSA"),
         (eckcdsa_sign, eckcdsa_verify, eckcdsa_genKeyPair, "ECKCDSA"),
         (ecfsdsa_sign, ecfsdsa_verify, genKeyPair, "ECFSDSA"),
         (ecrdsa_sign, ecrdsa_verify, genKeyPair, "ECRDSA"),
         (ecgdsa_sign, ecgdsa_verify, eckcdsa_genKeyPair, "ECGDSA"),
         (ecsdsa_sign, ecsdsa_verify, genKeyPair, "ECSDSA"),
         (ecosdsa_sign, ecosdsa_verify, genKeyPair, "ECOSDSA"), ]


curr_test = 0
def pretty_print_curr_test(num_test, total_gen_tests):
    num_decimal = int(math.log10(total_gen_tests))+1
    format_buf = "%0"+str(num_decimal)+"d/%0"+str(num_decimal)+"d"
    sys.stdout.write('\b'*((2*num_decimal)+1))
    sys.stdout.flush()
    sys.stdout.write(format_buf % (num_test, total_gen_tests))
    if num_test == total_gen_tests:
        print("")
    return

def gen_self_test(curve, hashfunc, sig_alg_sign, sig_alg_verify, sig_alg_genkeypair, num, hashfunc_name, sig_alg_name, total_gen_tests):
    global curr_test
    curr_test = curr_test + 1
    if num != 0:
        pretty_print_curr_test(curr_test, total_gen_tests)
    output_list = []
    for test_num in range(0, num):
        out_vectors = ""
        # Generate a random key pair
        keypair = sig_alg_genkeypair(curve)
        # Generate a random message with a random size
        size = getrandomint(256)
        if is_python_2():
            message = ''.join([random.choice(string.ascii_letters + string.digits) for n in xrange(size)])
        else:
            message = ''.join([random.choice(string.ascii_letters + string.digits) for n in range(size)])
        test_name = sig_alg_name + "_" + hashfunc_name + "_" + curve.name.upper() + "_" + str(test_num)
        # Sign the message
        (sig, k) = sig_alg_sign(hashfunc, keypair, message)
        # Check that everything is OK with a verify
        if sig_alg_verify(hashfunc, keypair, message, sig) != True:
            raise Exception("Error during self test generation: sig verify failed! "+test_name+ "   /  msg="+message+"   /   sig="+binascii.hexlify(sig)+"    /    k="+hex(k)+"   /   privkey.x="+hex(keypair.privkey.x))
        if sig_alg_name == "ECRDSA":
            out_vectors += "#ifndef USE_ISO14888_3_ECRDSA\n"
        # Now generate the test vector
        out_vectors += "#ifdef WITH_HASH_"+hashfunc_name.upper()+"\n"
        out_vectors += "#ifdef WITH_CURVE_"+curve.name.upper()+"\n"
        out_vectors += "#ifdef WITH_SIG_"+sig_alg_name.upper()+"\n"
        out_vectors += "/* "+test_name+" known test vectors */\n"
        out_vectors += "static int "+test_name+"_test_vectors_get_random(nn_t out, nn_src_t q)\n{\n"
        # k_buf MUST be exported padded to the length of q
        out_vectors += "\tconst u8 k_buf[] = "+bigint_to_C_array(k, getbytelen(curve.q))
        out_vectors += "\tint ret, cmp;\n\tret = nn_init_from_buf(out, k_buf, sizeof(k_buf)); EG(ret, err);\n\tret = nn_cmp(out, q, &cmp); EG(ret, err);\n\tret = (cmp >= 0) ? -1 : 0;\nerr:\n\treturn ret;\n}\n"
        out_vectors += "static const u8 "+test_name+"_test_vectors_priv_key[] = \n"+bigint_to_C_array(keypair.privkey.x, getbytelen(keypair.privkey.x))
        out_vectors += "static const u8 "+test_name+"_test_vectors_expected_sig[] = \n"+bigint_to_C_array(stringtoint(sig), len(sig))
        out_vectors += "static const ec_test_case "+test_name+"_test_case = {\n"
        out_vectors += "\t.name = \""+test_name+"\",\n"
        out_vectors += "\t.ec_str_p = &"+curve.name+"_str_params,\n"
        out_vectors += "\t.priv_key = "+test_name+"_test_vectors_priv_key,\n"
        out_vectors += "\t.priv_key_len = sizeof("+test_name+"_test_vectors_priv_key),\n"
        out_vectors += "\t.nn_random = "+test_name+"_test_vectors_get_random,\n"
        out_vectors += "\t.hash_type = "+hashfunc_name+",\n"
        out_vectors += "\t.msg = \""+message+"\",\n"
        out_vectors += "\t.msglen = "+str(len(message))+",\n"
        out_vectors += "\t.sig_type = "+sig_alg_name+",\n"
        out_vectors += "\t.exp_sig = "+test_name+"_test_vectors_expected_sig,\n"
        out_vectors += "\t.exp_siglen = sizeof("+test_name+"_test_vectors_expected_sig),\n};\n"
        out_vectors += "#endif /* WITH_HASH_"+hashfunc_name+" */\n"
        out_vectors += "#endif /* WITH_CURVE_"+curve.name+" */\n"
        out_vectors += "#endif /* WITH_SIG_"+sig_alg_name+" */\n"
        if sig_alg_name == "ECRDSA":
            out_vectors += "#endif /* !USE_ISO14888_3_ECRDSA */\n"
        out_name = ""
        if sig_alg_name == "ECRDSA":
            out_name += "#ifndef USE_ISO14888_3_ECRDSA"+"/* For "+test_name+" */\n"
        out_name += "#ifdef WITH_HASH_"+hashfunc_name.upper()+"/* For "+test_name+" */\n"
        out_name += "#ifdef WITH_CURVE_"+curve.name.upper()+"/* For "+test_name+" */\n"
        out_name += "#ifdef WITH_SIG_"+sig_alg_name.upper()+"/* For "+test_name+" */\n"
        out_name += "\t&"+test_name+"_test_case,\n"
        out_name += "#endif /* WITH_HASH_"+hashfunc_name+" for "+test_name+" */\n"
        out_name += "#endif /* WITH_CURVE_"+curve.name+" for "+test_name+" */\n"
        out_name += "#endif /* WITH_SIG_"+sig_alg_name+" for "+test_name+" */"
        if sig_alg_name == "ECRDSA":
            out_name += "\n#endif /* !USE_ISO14888_3_ECRDSA */"+"/* For "+test_name+" */"
        output_list.append((out_name, out_vectors))
        # In the specific case of ECRDSA, we also generate an ISO/IEC compatible test vector
        if sig_alg_name == "ECRDSA":
            out_vectors = ""
            (sig, k) = sig_alg_sign(hashfunc, keypair, message, use_iso14888_divergence=True)
            # Check that everything is OK with a verify
            if sig_alg_verify(hashfunc, keypair, message, sig, use_iso14888_divergence=True) != True:
                raise Exception("Error during self test generation: sig verify failed! "+test_name+ "   /  msg="+message+"   /   sig="+binascii.hexlify(sig)+"    /    k="+hex(k)+"   /   privkey.x="+hex(keypair.privkey.x))
            out_vectors += "#ifdef USE_ISO14888_3_ECRDSA\n"
            # Now generate the test vector
            out_vectors += "#ifdef WITH_HASH_"+hashfunc_name.upper()+"\n"
            out_vectors += "#ifdef WITH_CURVE_"+curve.name.upper()+"\n"
            out_vectors += "#ifdef WITH_SIG_"+sig_alg_name.upper()+"\n"
            out_vectors += "/* "+test_name+" known test vectors */\n"
            out_vectors += "static int "+test_name+"_test_vectors_get_random(nn_t out, nn_src_t q)\n{\n"
            # k_buf MUST be exported padded to the length of q
            out_vectors += "\tconst u8 k_buf[] = "+bigint_to_C_array(k, getbytelen(curve.q))
            out_vectors += "\tint ret, cmp;\n\tret = nn_init_from_buf(out, k_buf, sizeof(k_buf)); EG(ret, err);\n\tret = nn_cmp(out, q, &cmp); EG(ret, err);\n\tret = (cmp >= 0) ? -1 : 0;\nerr:\n\treturn ret;\n}\n"
            out_vectors += "static const u8 "+test_name+"_test_vectors_priv_key[] = \n"+bigint_to_C_array(keypair.privkey.x, getbytelen(keypair.privkey.x))
            out_vectors += "static const u8 "+test_name+"_test_vectors_expected_sig[] = \n"+bigint_to_C_array(stringtoint(sig), len(sig))
            out_vectors += "static const ec_test_case "+test_name+"_test_case = {\n"
            out_vectors += "\t.name = \""+test_name+"\",\n"
            out_vectors += "\t.ec_str_p = &"+curve.name+"_str_params,\n"
            out_vectors += "\t.priv_key = "+test_name+"_test_vectors_priv_key,\n"
            out_vectors += "\t.priv_key_len = sizeof("+test_name+"_test_vectors_priv_key),\n"
            out_vectors += "\t.nn_random = "+test_name+"_test_vectors_get_random,\n"
            out_vectors += "\t.hash_type = "+hashfunc_name+",\n"
            out_vectors += "\t.msg = \""+message+"\",\n"
            out_vectors += "\t.msglen = "+str(len(message))+",\n"
            out_vectors += "\t.sig_type = "+sig_alg_name+",\n"
            out_vectors += "\t.exp_sig = "+test_name+"_test_vectors_expected_sig,\n"
            out_vectors += "\t.exp_siglen = sizeof("+test_name+"_test_vectors_expected_sig),\n};\n"
            out_vectors += "#endif /* WITH_HASH_"+hashfunc_name+" */\n"
            out_vectors += "#endif /* WITH_CURVE_"+curve.name+" */\n"
            out_vectors += "#endif /* WITH_SIG_"+sig_alg_name+" */\n"
            out_vectors += "#endif /* USE_ISO14888_3_ECRDSA */\n"
            out_name = ""
            out_name += "#ifdef USE_ISO14888_3_ECRDSA"+"/* For "+test_name+" */\n"
            out_name += "#ifdef WITH_HASH_"+hashfunc_name.upper()+"/* For "+test_name+" */\n"
            out_name += "#ifdef WITH_CURVE_"+curve.name.upper()+"/* For "+test_name+" */\n"
            out_name += "#ifdef WITH_SIG_"+sig_alg_name.upper()+"/* For "+test_name+" */\n"
            out_name += "\t&"+test_name+"_test_case,\n"
            out_name += "#endif /* WITH_HASH_"+hashfunc_name+" for "+test_name+" */\n"
            out_name += "#endif /* WITH_CURVE_"+curve.name+" for "+test_name+" */\n"
            out_name += "#endif /* WITH_SIG_"+sig_alg_name+" for "+test_name+" */\n"
            out_name += "#endif /* USE_ISO14888_3_ECRDSA */"+"/* For "+test_name+" */"
            output_list.append((out_name, out_vectors))

    return output_list

def gen_self_tests(curve, num):
    global curr_test
    curr_test = 0
    total_gen_tests = len(all_hash_funcs) * len(all_sig_algs)
    vectors = [[ gen_self_test(curve, hashf, sign, verify, genkp, num, hash_name, sig_alg_name, total_gen_tests)
               for (hashf, hash_name) in all_hash_funcs ] for (sign, verify, genkp, sig_alg_name) in all_sig_algs ]
    return vectors

##########################################################
### ASN.1 stuff
def parse_DER_extract_size(derbuf):
    # Extract the size
    if ord(derbuf[0]) & 0x80 != 0:
        encoding_len_bytes = ord(derbuf[0]) & ~0x80
        # Skip
        base = 1
    else:
        encoding_len_bytes = 1
        base = 0
    if len(derbuf) < encoding_len_bytes+1:
        return (False, 0, 0)
    else:
        length = stringtoint(derbuf[base:base+encoding_len_bytes])
        if len(derbuf) < length+encoding_len_bytes:
            return (False, 0, 0)
        else:
            return (True, encoding_len_bytes+base, length)

def extract_DER_object(derbuf, object_tag):
    # Check type
    if ord(derbuf[0]) != object_tag:
        # Not the type we expect ...
        return (False, 0, "")
    else:
        derbuf = derbuf[1:]
        # Extract the size
        (check, encoding_len, size) = parse_DER_extract_size(derbuf)
        if check == False:
            return (False, 0, "")
        else:
            if len(derbuf) < encoding_len + size:
                return (False, 0, "")
            else:
                return (True, size+encoding_len+1, derbuf[encoding_len:encoding_len+size])

def extract_DER_sequence(derbuf):
    return extract_DER_object(derbuf, 0x30)

def extract_DER_integer(derbuf):
    return extract_DER_object(derbuf, 0x02)

def extract_DER_octetstring(derbuf):
    return extract_DER_object(derbuf, 0x04)

def extract_DER_bitstring(derbuf):
    return extract_DER_object(derbuf, 0x03)

def extract_DER_oid(derbuf):
    return extract_DER_object(derbuf, 0x06)

# See ECParameters sequence in RFC 3279
def parse_DER_ECParameters(derbuf):
    # XXX: this is a very ugly way of extracting the information
    # regarding an EC curve, but since the ASN.1 structure is quite
    # "static", this might be sufficient without embedding a full
    # ASN.1 parser ...
    # Default return (a, b, prime, order, cofactor, gx, gy)
    default_ret = (0, 0, 0, 0, 0, 0, 0)
    # Get ECParameters wrapping sequence
    (check, size_ECParameters, ECParameters) = extract_DER_sequence(derbuf)
    if check == False:
        return (False, default_ret)
    # Get integer
    (check, size_ECPVer, ECPVer) = extract_DER_integer(ECParameters)
    if check == False:
        return (False, default_ret)
    # Get sequence
    (check, size_FieldID, FieldID) = extract_DER_sequence(ECParameters[size_ECPVer:])
    if check == False:
        return (False, default_ret)
    # Get OID
    (check, size_Oid, Oid) = extract_DER_oid(FieldID)
    if check == False:
        return (False, default_ret)
    # Does the OID correspond to a prime field?
    if(Oid != "\x2A\x86\x48\xCE\x3D\x01\x01"):
        print("DER parse error: only prime fields are supported ...")
        return (False, default_ret)
    # Get prime p of prime field
    (check, size_P, P) = extract_DER_integer(FieldID[size_Oid:])
    if check == False:
        return (False, default_ret)
    # Get curve (sequence)
    (check, size_Curve, Curve) = extract_DER_sequence(ECParameters[size_ECPVer+size_FieldID:])
    if check == False:
        return (False, default_ret)
    # Get A in curve
    (check, size_A, A) = extract_DER_octetstring(Curve)
    if check == False:
        return (False, default_ret)
    # Get B in curve
    (check, size_B, B) = extract_DER_octetstring(Curve[size_A:])
    if check == False:
        return (False, default_ret)
    # Get ECPoint
    (check, size_ECPoint, ECPoint) = extract_DER_octetstring(ECParameters[size_ECPVer+size_FieldID+size_Curve:])
    if check == False:
        return (False, default_ret)
    # Get Order
    (check, size_Order, Order) = extract_DER_integer(ECParameters[size_ECPVer+size_FieldID+size_Curve+size_ECPoint:])
    if check == False:
        return (False, default_ret)
    # Get Cofactor
    (check, size_Cofactor, Cofactor) = extract_DER_integer(ECParameters[size_ECPVer+size_FieldID+size_Curve+size_ECPoint+size_Order:])
    if check == False:
        return (False, default_ret)
    # If we end up here, everything is OK, we can extract all our elements
    prime = stringtoint(P)
    a = stringtoint(A)
    b = stringtoint(B)
    order = stringtoint(Order)
    cofactor = stringtoint(Cofactor)
    # Extract Gx and Gy, see X9.62-1998
    if len(ECPoint) < 1:
        return (False, default_ret)
    ECPoint_type = ord(ECPoint[0])
    if (ECPoint_type == 0x04) or (ECPoint_type == 0x06) or (ECPoint_type == 0x07):
        # Uncompressed and hybrid points
        if len(ECPoint[1:]) % 2 != 0:
            return (False, default_ret)
        ECPoint = ECPoint[1:]
        gx = stringtoint(ECPoint[:int(len(ECPoint)/2)])
        gy = stringtoint(ECPoint[int(len(ECPoint)/2):])
    elif (ECPoint_type == 0x02) or (ECPoint_type == 0x03):
        # Compressed point: uncompress it, see X9.62-1998 section 4.2.1
        ECPoint = ECPoint[1:]
        gx = stringtoint(ECPoint)
        alpha = (pow(gx, 3, prime) + (a * gx) + b) % prime
        beta = mod_sqrt(alpha, prime)
        if (beta == None) or ((beta == 0) and (alpha != 0)):
            return (False, 0)
        if (beta & 0x1) == (ECPoint_type & 0x1):
            gy = beta
        else:
            gy = prime - beta
    else:
        print("DER parse error: hybrid points are unsupported!")
        return (False, default_ret)
    return (True, (a, b, prime, order, cofactor, gx, gy))

##########################################################
### Text and format helpers
def bigint_to_C_array(bint, size):
    """
    Format a python big int to a C hex array
    """
    hexstr = format(int(bint), 'x')
    # Left pad to the size!
    hexstr = ("0"*int((2*size)-len(hexstr)))+hexstr
    hexstr = ("0"*(len(hexstr) % 2))+hexstr
    out_str = "{\n"
    for i in range(0, len(hexstr) - 1, 2):
        if (i%16 == 0):
            if(i!=0):
                out_str += "\n"
            out_str += "\t"
        out_str += "0x"+hexstr[i:i+2]+", "
    out_str += "\n};\n"
    return out_str

def check_in_file(fname, pat):
    # See if the pattern is in the file.
    with open(fname) as f:
        if not any(re.search(pat, line) for line in f):
            return False # pattern does not occur in file so we are done.
        else:
            return True

def num_patterns_in_file(fname, pat):
    num_pat = 0
    with open(fname) as f:
        for line in f:
            if re.search(pat, line):
                num_pat = num_pat+1
    return num_pat

def file_replace_pattern(fname, pat, s_after):
    # first, see if the pattern is even in the file.
    with open(fname) as f:
        if not any(re.search(pat, line) for line in f):
            return # pattern does not occur in file so we are done.

    # pattern is in the file, so perform replace operation.
    with open(fname) as f:
        out_fname = fname + ".tmp"
        out = open(out_fname, "w")
        for line in f:
            out.write(re.sub(pat, s_after, line))
        out.close()
        os.rename(out_fname, fname)

def file_remove_pattern(fname, pat):
    # first, see if the pattern is even in the file.
    with open(fname) as f:
        if not any(re.search(pat, line) for line in f):
            return # pattern does not occur in file so we are done.

    # pattern is in the file, so perform remove operation.
    with open(fname) as f:
        out_fname = fname + ".tmp"
        out = open(out_fname, "w")
        for line in f:
            if not re.search(pat, line):
                out.write(line)
        out.close()

    if os.path.exists(fname):
        remove_file(fname)
    os.rename(out_fname, fname)

def remove_file(fname):
    # Remove file
    os.remove(fname)

def remove_files_pattern(fpattern):
    [remove_file(x) for x in glob.glob(fpattern)]

def buffer_remove_pattern(buff, pat):
    if is_python_2() == False:
        buff = buff.decode('latin-1')
    if re.search(pat, buff) == None:
        return (False, buff) # pattern does not occur in file so we are done.
    # Remove the pattern
    buff = re.sub(pat, "", buff)
    return (True, buff)

def is_base64(s):
    s = ''.join([s.strip() for s in s.split("\n")])
    try:
        enc = base64.b64encode(base64.b64decode(s)).strip()
        if type(enc) is bytes:
            return enc == s.encode('latin-1')
        else:
            return enc == s
    except TypeError:
        return False

### Curve helpers
def export_curve_int(curvename, intname, bigint, size):
    if bigint == None:
        out  = "static const u8 "+curvename+"_"+intname+"[] = {\n\t0x00,\n};\n"
        out += "TO_EC_STR_PARAM_FIXED_SIZE("+curvename+"_"+intname+", 0);\n\n"
    else:
        out  = "static const u8 "+curvename+"_"+intname+"[] = "+bigint_to_C_array(bigint, size)+"\n"
        out += "TO_EC_STR_PARAM("+curvename+"_"+intname+");\n\n"
    return out

def export_curve_string(curvename, stringname, stringvalue):
    out  = "static const u8 "+curvename+"_"+stringname+"[] = \""+stringvalue+"\";\n"
    out += "TO_EC_STR_PARAM("+curvename+"_"+stringname+");\n\n"
    return out

def export_curve_struct(curvename, paramname, paramnamestr):
    return "\t."+paramname+" = &"+curvename+"_"+paramnamestr+"_str_param, \n"

def curve_params(name, prime, pbitlen, a, b, gx, gy, order, cofactor, oid, alpha_montgomery, gamma_montgomery, alpha_edwards):
    """
    Take as input some elliptic curve parameters and generate the
    C parameters in a string
    """
    bytesize = int(pbitlen / 8)
    if pbitlen % 8 != 0:
        bytesize += 1
    # Compute the rounded word size for each word size
    if bytesize % 8 != 0:
        wordsbitsize64 = 8*((int(bytesize/8)+1)*8)
    else:
        wordsbitsize64 = 8*bytesize
    if bytesize % 4 != 0:
        wordsbitsize32 = 8*((int(bytesize/4)+1)*4)
    else:
        wordsbitsize32 = 8*bytesize
    if bytesize % 2 != 0:
        wordsbitsize16 = 8*((int(bytesize/2)+1)*2)
    else:
        wordsbitsize16 = 8*bytesize
    # Compute some parameters
    (r64, r_square64, mpinv64) = compute_monty_coef(prime, wordsbitsize64, 64)
    (r32, r_square32, mpinv32) = compute_monty_coef(prime, wordsbitsize32, 32)
    (r16, r_square16, mpinv16) = compute_monty_coef(prime, wordsbitsize16, 16)
    # Compute p_reciprocal for each word size
    (pshift64, primenorm64, p_reciprocal64) = compute_div_coef(prime, wordsbitsize64, 64)
    (pshift32, primenorm32, p_reciprocal32) = compute_div_coef(prime, wordsbitsize32, 32)
    (pshift16, primenorm16, p_reciprocal16) = compute_div_coef(prime, wordsbitsize16, 16)
    # Compute the number of points on the curve
    npoints = order * cofactor

    # Now output the parameters
    ec_params_string =  "#include <libecc/lib_ecc_config.h>\n"
    ec_params_string += "#ifdef WITH_CURVE_"+name.upper()+"\n\n"
    ec_params_string += "#ifndef __EC_PARAMS_"+name.upper()+"_H__\n"
    ec_params_string += "#define __EC_PARAMS_"+name.upper()+"_H__\n"
    ec_params_string += "#include <libecc/curves/known/ec_params_external.h>\n"
    ec_params_string += export_curve_int(name, "p", prime, bytesize)

    ec_params_string += "#define CURVE_"+name.upper()+"_P_BITLEN "+str(pbitlen)+"\n"
    ec_params_string += export_curve_int(name, "p_bitlen", pbitlen, getbytelen(pbitlen))

    ec_params_string += "#if (WORD_BYTES == 8)     /* 64-bit words */\n"
    ec_params_string += export_curve_int(name, "r", r64, getbytelen(r64))
    ec_params_string += export_curve_int(name, "r_square", r_square64, getbytelen(r_square64))
    ec_params_string += export_curve_int(name, "mpinv", mpinv64, getbytelen(mpinv64))
    ec_params_string += export_curve_int(name, "p_shift", pshift64, getbytelen(pshift64))
    ec_params_string += export_curve_int(name, "p_normalized", primenorm64, getbytelen(primenorm64))
    ec_params_string += export_curve_int(name, "p_reciprocal", p_reciprocal64, getbytelen(p_reciprocal64))
    ec_params_string += "#elif (WORD_BYTES == 4)   /* 32-bit words */\n"
    ec_params_string += export_curve_int(name, "r", r32, getbytelen(r32))
    ec_params_string += export_curve_int(name, "r_square", r_square32, getbytelen(r_square32))
    ec_params_string += export_curve_int(name, "mpinv", mpinv32, getbytelen(mpinv32))
    ec_params_string += export_curve_int(name, "p_shift", pshift32, getbytelen(pshift32))
    ec_params_string += export_curve_int(name, "p_normalized", primenorm32, getbytelen(primenorm32))
    ec_params_string += export_curve_int(name, "p_reciprocal", p_reciprocal32, getbytelen(p_reciprocal32))
    ec_params_string += "#elif (WORD_BYTES == 2)   /* 16-bit words */\n"
    ec_params_string += export_curve_int(name, "r", r16, getbytelen(r16))
    ec_params_string += export_curve_int(name, "r_square", r_square16, getbytelen(r_square16))
    ec_params_string += export_curve_int(name, "mpinv", mpinv16, getbytelen(mpinv16))
    ec_params_string += export_curve_int(name, "p_shift", pshift16, getbytelen(pshift16))
    ec_params_string += export_curve_int(name, "p_normalized", primenorm16, getbytelen(primenorm16))
    ec_params_string += export_curve_int(name, "p_reciprocal", p_reciprocal16, getbytelen(p_reciprocal16))
    ec_params_string += "#else                     /* unknown word size */\n"
    ec_params_string += "#error \"Unsupported word size\"\n"
    ec_params_string += "#endif\n\n"

    ec_params_string += export_curve_int(name, "a", a, bytesize)
    ec_params_string += export_curve_int(name, "b", b, bytesize)

    curve_order_bitlen = getbitlen(npoints)
    ec_params_string += "#define CURVE_"+name.upper()+"_CURVE_ORDER_BITLEN "+str(curve_order_bitlen)+"\n"
    ec_params_string += export_curve_int(name, "curve_order", npoints, getbytelen(npoints))

    ec_params_string += export_curve_int(name, "gx", gx, bytesize)
    ec_params_string += export_curve_int(name, "gy", gy, bytesize)
    ec_params_string += export_curve_int(name, "gz", 0x01, bytesize)

    qbitlen = getbitlen(order)

    ec_params_string += export_curve_int(name, "gen_order", order, getbytelen(order))
    ec_params_string += "#define CURVE_"+name.upper()+"_Q_BITLEN "+str(qbitlen)+"\n"
    ec_params_string += export_curve_int(name, "gen_order_bitlen", qbitlen, getbytelen(qbitlen))

    ec_params_string += export_curve_int(name, "cofactor", cofactor, getbytelen(cofactor))

    ec_params_string += export_curve_int(name, "alpha_montgomery", alpha_montgomery, getbytelen(alpha_montgomery))
    ec_params_string += export_curve_int(name, "gamma_montgomery", gamma_montgomery, getbytelen(gamma_montgomery))
    ec_params_string += export_curve_int(name, "alpha_edwards", alpha_edwards, getbytelen(alpha_edwards))

    ec_params_string += export_curve_string(name, "name", name.upper());

    if oid == None:
        oid = ""
    ec_params_string += export_curve_string(name, "oid", oid);

    ec_params_string += "static const ec_str_params "+name+"_str_params = {\n"+\
    export_curve_struct(name, "p", "p") +\
    export_curve_struct(name, "p_bitlen", "p_bitlen") +\
    export_curve_struct(name, "r", "r") +\
    export_curve_struct(name, "r_square", "r_square") +\
    export_curve_struct(name, "mpinv", "mpinv") +\
    export_curve_struct(name, "p_shift", "p_shift") +\
    export_curve_struct(name, "p_normalized", "p_normalized") +\
    export_curve_struct(name, "p_reciprocal", "p_reciprocal") +\
    export_curve_struct(name, "a", "a") +\
    export_curve_struct(name, "b", "b") +\
    export_curve_struct(name, "curve_order", "curve_order") +\
    export_curve_struct(name, "gx", "gx") +\
    export_curve_struct(name, "gy", "gy") +\
    export_curve_struct(name, "gz", "gz") +\
    export_curve_struct(name, "gen_order", "gen_order") +\
    export_curve_struct(name, "gen_order_bitlen", "gen_order_bitlen") +\
    export_curve_struct(name, "cofactor", "cofactor") +\
    export_curve_struct(name, "alpha_montgomery", "alpha_montgomery") +\
    export_curve_struct(name, "gamma_montgomery", "gamma_montgomery") +\
    export_curve_struct(name, "alpha_edwards", "alpha_edwards") +\
    export_curve_struct(name, "oid", "oid") +\
    export_curve_struct(name, "name", "name")
    ec_params_string += "};\n\n"

    ec_params_string += "/*\n"+\
    " * Compute max bit length of all curves for p and q\n"+\
    " */\n"+\
    "#ifndef CURVES_MAX_P_BIT_LEN\n"+\
    "#define CURVES_MAX_P_BIT_LEN    0\n"+\
    "#endif\n"+\
    "#if (CURVES_MAX_P_BIT_LEN < CURVE_"+name.upper()+"_P_BITLEN)\n"+\
    "#undef CURVES_MAX_P_BIT_LEN\n"+\
    "#define CURVES_MAX_P_BIT_LEN CURVE_"+name.upper()+"_P_BITLEN\n"+\
    "#endif\n"+\
    "#ifndef CURVES_MAX_Q_BIT_LEN\n"+\
    "#define CURVES_MAX_Q_BIT_LEN    0\n"+\
    "#endif\n"+\
    "#if (CURVES_MAX_Q_BIT_LEN < CURVE_"+name.upper()+"_Q_BITLEN)\n"+\
    "#undef CURVES_MAX_Q_BIT_LEN\n"+\
    "#define CURVES_MAX_Q_BIT_LEN CURVE_"+name.upper()+"_Q_BITLEN\n"+\
    "#endif\n"+\
    "#ifndef CURVES_MAX_CURVE_ORDER_BIT_LEN\n"+\
    "#define CURVES_MAX_CURVE_ORDER_BIT_LEN    0\n"+\
    "#endif\n"+\
    "#if (CURVES_MAX_CURVE_ORDER_BIT_LEN < CURVE_"+name.upper()+"_CURVE_ORDER_BITLEN)\n"+\
    "#undef CURVES_MAX_CURVE_ORDER_BIT_LEN\n"+\
    "#define CURVES_MAX_CURVE_ORDER_BIT_LEN CURVE_"+name.upper()+"_CURVE_ORDER_BITLEN\n"+\
    "#endif\n\n"

    ec_params_string += "/*\n"+\
    " * Compute and adapt max name and oid length\n"+\
    " */\n"+\
    "#ifndef MAX_CURVE_OID_LEN\n"+\
    "#define MAX_CURVE_OID_LEN 0\n"+\
    "#endif\n"+\
    "#ifndef MAX_CURVE_NAME_LEN\n"+\
    "#define MAX_CURVE_NAME_LEN 0\n"+\
    "#endif\n"+\
    "#if (MAX_CURVE_OID_LEN < "+str(len(oid)+1)+")\n"+\
    "#undef MAX_CURVE_OID_LEN\n"+\
    "#define MAX_CURVE_OID_LEN "+str(len(oid)+1)+"\n"+\
    "#endif\n"+\
    "#if (MAX_CURVE_NAME_LEN < "+str(len(name.upper())+1)+")\n"+\
    "#undef MAX_CURVE_NAME_LEN\n"+\
    "#define MAX_CURVE_NAME_LEN "+str(len(name.upper())+1)+"\n"+\
    "#endif\n\n"

    ec_params_string += "#endif /* __EC_PARAMS_"+name.upper()+"_H__ */\n\n"+"#endif /* WITH_CURVE_"+name.upper()+" */\n"

    return ec_params_string

def usage():
    print("This script is intented to *statically* expand the ECC library with user defined curves.")
    print("By statically we mean that the source code of libecc is expanded with new curves parameters through")
    print("automatic code generation filling place holders in the existing code base of the library. Though the")
    print("choice of static code generation versus dynamic curves import (such as what OpenSSL does) might be")
    print("argued, this choice has been driven by simplicity and security design decisions: we want libecc to have")
    print("all its parameters (such as memory consumption) set at compile time and statically adapted to the curves.")
    print("Since libecc only supports curves over prime fields, the script can only add this kind of curves.")
    print("This script implements elliptic curves and ISO signature algorithms from scratch over Python's multi-precision")
    print("big numbers library. Addition and doubling over curves use naive formulas. Please DO NOT use the functions of this")
    print("script for production code: they are not securely implemented and are very inefficient. Their only purpose is to expand")
    print("libecc and produce test vectors.")
    print("")
    print("In order to add a curve, there are two ways:")
    print("Adding a user defined curve with explicit parameters:")
    print("-----------------------------------------------------")
    print(sys.argv[0]+" --name=\"YOURCURVENAME\" --prime=... --order=... --a=... --b=... --gx=... --gy=... --cofactor=... --oid=THEOID")
    print("\t> name: name of the curve in the form of a string")
    print("\t> prime: prime number representing the curve prime field")
    print("\t> order: prime number representing the generator order")
    print("\t> cofactor: cofactor of the curve")
    print("\t> a: 'a' coefficient of the short Weierstrass equation of the curve")
    print("\t> b: 'b' coefficient of the short Weierstrass equation of the curve")
    print("\t> gx: x coordinate of the generator G")
    print("\t> gy: y coordinate of the generator G")
    print("\t> oid: optional OID of the curve")
    print("  Notes:")
    print("  ******")
    print("\t1) These elements are verified to indeed satisfy the curve equation.")
    print("\t2) All the numbers can be given either in decimal or hexadecimal format with a prepending '0x'.")
    print("\t3) The script automatically generates all the necessary files for the curve to be included in the library." )
    print("\tYou will find the new curve definition in the usual 'lib_ecc_config.h' file (one can activate it or not at compile time).")
    print("")
    print("Adding a user defined curve through RFC3279 ASN.1 parameters:")
    print("-------------------------------------------------------------")
    print(sys.argv[0]+" --name=\"YOURCURVENAME\" --ECfile=... --oid=THEOID")
    print("\t> ECfile: the DER or PEM encoded file containing the curve parameters (see RFC3279)")
    print("  Notes:")
    print("\tCurve parameters encoded in DER or PEM format can be generated with tools like OpenSSL (among others). As an illustrative example,")
    print("\tone can list all the supported curves under OpenSSL with:")
    print("\t  $ openssl ecparam -list_curves")
    print("\tOnly the listed so called \"prime\" curves are supported. Then, one can extract an explicit curve representation in ASN.1")
    print("\tas defined in RFC3279, for example for BRAINPOOLP320R1:")
    print("\t  $ openssl ecparam -param_enc explicit -outform DER -name brainpoolP320r1 -out brainpoolP320r1.der")
    print("")
    print("Removing user defined curves:")
    print("-----------------------------")
    print("\t*All the user defined curves can be removed with the --remove-all toggle.")
    print("\t*A specific named user define curve can be removed with the --remove toggle: in this case the --name option is used to ")
    print("\tlocate which named curve must be deleted.")
    print("")
    print("Test vectors:")
    print("-------------")
    print("\tTest vectors can be automatically generated and added to the library self tests when providing the --add-test-vectors=X toggle.")
    print("\tIn this case, X test vectors will be generated for *each* (curve, sign algorithm, hash algorithm) 3-uplet (beware of combinatorial")
    print("\tissues when X is big!). These tests are transparently added and compiled with the self tests.")
    return

def get_int(instring):
    if len(instring) == 0:
        return 0
    if len(instring) >= 2:
        if instring[:2] == "0x":
            return int(instring, 16)
    return int(instring)

def parse_cmd_line(args):
    """
    Get elliptic curve parameters from command line
    """
    name = oid = prime = a = b = gx = gy = g = order = cofactor = ECfile = remove = remove_all = add_test_vectors = None
    alpha_montgomery = gamma_montgomery = alpha_edwards = None
    try:
        opts, args = getopt.getopt(sys.argv[1:], ":h", ["help", "remove", "remove-all", "name=", "prime=", "a=", "b=", "generator=", "gx=", "gy=", "order=", "cofactor=", "alpha_montgomery=","gamma_montgomery=", "alpha_edwards=", "ECfile=", "oid=", "add-test-vectors="])
    except getopt.GetoptError as err:
        # print help information and exit:
        print(err) # will print something like "option -a not recognized"
        usage()
        return False
    for o, arg in opts:
        if o in ("-h", "--help"):
            usage()
            return True
        elif o in ("--name"):
            name = arg
            # Prepend the custom string before name to avoid any collision
            name = "user_defined_"+name
            # Replace any unwanted name char
            name = re.sub("\-", "_", name)
        elif o in ("--oid="):
            oid = arg
        elif o in ("--prime"):
            prime = get_int(arg.replace(' ', ''))
        elif o in ("--a"):
            a = get_int(arg.replace(' ', ''))
        elif o in ("--b"):
            b = get_int(arg.replace(' ', ''))
        elif o in ("--gx"):
            gx = get_int(arg.replace(' ', ''))
        elif o in ("--gy"):
            gy = get_int(arg.replace(' ', ''))
        elif o in ("--generator"):
            g = arg.replace(' ', '')
        elif o in ("--order"):
            order = get_int(arg.replace(' ', ''))
        elif o in ("--cofactor"):
            cofactor = get_int(arg.replace(' ', ''))
        elif o in ("--alpha_montgomery"):
            alpha_montgomery = get_int(arg.replace(' ', ''))
        elif o in ("--gamma_montgomery"):
            gamma_montgomery = get_int(arg.replace(' ', ''))
        elif o in ("--alpha_edwards"):
            alpha_edwards = get_int(arg.replace(' ', ''))
        elif o in ("--remove"):
            remove = True
        elif o in ("--remove-all"):
            remove_all = True
        elif o in ("--add-test-vectors"):
            add_test_vectors = get_int(arg.replace(' ', ''))
        elif o in ("--ECfile"):
            ECfile = arg
        else:
            print("unhandled option")
            usage()
            return False

    # File paths
    script_path = os.path.abspath(os.path.dirname(sys.argv[0])) + "/"
    ec_params_path = script_path + "../include/libecc/curves/user_defined/"
    curves_list_path = script_path + "../include/libecc/curves/"
    lib_ecc_types_path = script_path + "../include/libecc/"
    lib_ecc_config_path = script_path + "../include/libecc/"
    ec_self_tests_path = script_path + "../src/tests/"
    meson_options_path = script_path + "../"

    # If remove is True, we have been asked to remove already existing user defined curves
    if remove == True:
        if name == None:
            print("--remove option expects a curve name provided with --name")
            return False
        asked = ""
        while asked != "y" and asked != "n":
            asked = get_user_input("You asked to remove everything related to user defined "+name.replace("user_defined_", "")+" curve. Enter y to confirm, n to cancel [y/n]. ")
        if asked == "n":
            print("NOT removing curve "+name.replace("user_defined_", "")+" (cancelled).")
            return True
        # Remove any user defined stuff with given name
        print("Removing user defined curve "+name.replace("user_defined_", "")+" ...")
        if name == None:
            print("Error: you must provide a curve name with --remove")
            return False
        file_remove_pattern(curves_list_path + "curves_list.h", ".*"+name+".*")
        file_remove_pattern(curves_list_path + "curves_list.h", ".*"+name.upper()+".*")
        file_remove_pattern(lib_ecc_types_path + "lib_ecc_types.h", ".*"+name.upper()+".*")
        file_remove_pattern(lib_ecc_config_path + "lib_ecc_config.h", ".*"+name.upper()+".*")
        file_remove_pattern(ec_self_tests_path + "ec_self_tests_core.h", ".*"+name+".*")
        file_remove_pattern(ec_self_tests_path + "ec_self_tests_core.h", ".*"+name.upper()+".*")
        file_remove_pattern(meson_options_path + "meson.options", ".*"+name.lower()+".*")
        try:
            remove_file(ec_params_path + "ec_params_"+name+".h")
        except:
            print("Error: curve name "+name+" does not seem to be present in the sources!")
            return False
        try:
            remove_file(ec_self_tests_path + "ec_self_tests_core_"+name+".h")
        except:
            print("Warning: curve name "+name+" self tests do not seem to be present ...")
            return True
        return True
    if remove_all == True:
        asked = ""
        while asked != "y" and asked != "n":
            asked = get_user_input("You asked to remove everything related to ALL user defined curves. Enter y to confirm, n to cancel [y/n]. ")
        if asked == "n":
            print("NOT removing user defined curves (cancelled).")
            return True
        # Remove any user defined stuff with given name
        print("Removing ALL user defined curves ...")
        # Remove any user defined stuff (whatever name)
        file_remove_pattern(curves_list_path + "curves_list.h", ".*user_defined.*")
        file_remove_pattern(curves_list_path + "curves_list.h", ".*USER_DEFINED.*")
        file_remove_pattern(lib_ecc_types_path + "lib_ecc_types.h", ".*USER_DEFINED.*")
        file_remove_pattern(lib_ecc_config_path + "lib_ecc_config.h", ".*USER_DEFINED.*")
        file_remove_pattern(ec_self_tests_path + "ec_self_tests_core.h", ".*USER_DEFINED.*")
        file_remove_pattern(ec_self_tests_path + "ec_self_tests_core.h", ".*user_defined.*")
        file_remove_pattern(meson_options_path + "meson.options", ".*user_defined.*")
        remove_files_pattern(ec_params_path + "ec_params_user_defined_*.h")
        remove_files_pattern(ec_self_tests_path + "ec_self_tests_core_user_defined_*.h")
        return True

    # If a g is provided, split it in two gx and gy
    if g != None:
        if (len(g)/2)%2 == 0:
            gx = get_int(g[:len(g)/2])
            gy = get_int(g[len(g)/2:])
        else:
            # This is probably a generator encapsulated in a bit string
            if g[0:2] != "04":
                print("Error: provided generator g is not conforming!")
                return False
            else:
                g = g[2:]
                gx = get_int(g[:len(g)/2])
                gy = get_int(g[len(g)/2:])
    if ECfile != None:
        # ASN.1 DER input incompatible with other options
        if (prime != None) or (a != None) or (b != None) or (gx != None) or (gy != None) or (order != None) or (cofactor != None):
            print("Error: option ECfile incompatible with explicit (prime, a, b, gx, gy, order, cofactor) options!")
            return False
        # We need at least a name
        if (name == None):
            print("Error: option ECfile needs a curve name!")
            return False
        # Open the file
        try:
            buf = open(ECfile, 'rb').read()
        except:
            print("Error: cannot open ECfile file "+ECfile)
            return False
        # Check if we have a PEM or a DER file
        (check, derbuf) = buffer_remove_pattern(buf, "-----.*-----")
        if (check == True):
            # This a PEM file, proceed with base64 decoding
            if(is_base64(derbuf) == False):
                print("Error: error when decoding ECfile file "+ECfile+" (seems to be PEM, but failed to decode)")
                return False
            derbuf = base64.b64decode(derbuf)
        (check, (a, b, prime, order, cofactor, gx, gy)) = parse_DER_ECParameters(derbuf)
        if (check == False):
            print("Error: error when parsing ECfile file "+ECfile+" (malformed or unsupported ASN.1)")
            return False

    else:
        if (prime == None) or (a == None) or (b == None) or (gx == None) or (gy == None) or (order == None) or (cofactor == None) or (name == None):
            err_string = (prime == None)*"prime "+(a == None)*"a "+(b == None)*"b "+(gx == None)*"gx "+(gy == None)*"gy "+(order == None)*"order "+(cofactor == None)*"cofactor "+(name == None)*"name "
            print("Error: missing "+err_string+" in explicit curve definition (name, prime, a, b, gx, gy, order, cofactor)!")
            print("See the help with -h or --help")
            return False

    # Some sanity checks here
    # Check that prime is indeed a prime
    if is_probprime(prime) == False:
        print("Error: given prime is *NOT* prime!")
        return False
    if is_probprime(order) == False:
        print("Error: given order is *NOT* prime!")
        return False
    if (a > prime) or (b > prime) or (gx > prime) or (gy > prime):
        err_string = (a > prime)*"a "+(b > prime)*"b "+(gx > prime)*"gx "+(gy > prime)*"gy "
        print("Error: "+err_string+"is > prime")
        return False
    # Check that the provided generator is on the curve
    if pow(gy, 2, prime) != ((pow(gx, 3, prime) + (a*gx) + b) % prime):
        print("Error: the given parameters (prime, a, b, gx, gy) do not verify the elliptic curve equation!")
        return False

    # Check Montgomery and Edwards transfer coefficients
    if ((alpha_montgomery != None) and (gamma_montgomery == None)) or ((alpha_montgomery == None) and (gamma_montgomery != None)):
        print("Error: alpha_montgomery and gamma_montgomery must be both defined if used!")
        return False
    if (alpha_edwards != None):
        if (alpha_montgomery == None) or (gamma_montgomery == None):
            print("Error: alpha_edwards needs alpha_montgomery and gamma_montgomery to be both defined if used!")
            return False

    # Now that we have our parameters, call the function to get bitlen
    pbitlen = getbitlen(prime)
    ec_params = curve_params(name, prime, pbitlen, a, b, gx, gy, order, cofactor, oid, alpha_montgomery, gamma_montgomery, alpha_edwards)
    # Check if there is a name collision somewhere
    if os.path.exists(ec_params_path + "ec_params_"+name+".h") == True :
        print("Error: file %s already exists!" % (ec_params_path + "ec_params_"+name+".h"))
        return False
    if (check_in_file(curves_list_path + "curves_list.h", "ec_params_"+name+"_str_params") == True) or (check_in_file(curves_list_path + "curves_list.h", "WITH_CURVE_"+name.upper()+"\n") == True) or (check_in_file(lib_ecc_types_path + "lib_ecc_types.h", "WITH_CURVE_"+name.upper()+"\n") == True):
        print("Error: name %s already exists in files" % ("ec_params_"+name))
        return False
    # Create a new file with the parameters
    if not os.path.exists(ec_params_path):
        # Create the "user_defined" folder if it does not exist
        os.mkdir(ec_params_path)
    f = open(ec_params_path + "ec_params_"+name+".h", 'w')
    f.write(ec_params)
    f.close()
    # Include the file in curves_list.h
    magic = "ADD curves header here"
    magic_re = "\/\* "+magic+" \*\/"
    magic_back = "/* "+magic+" */"
    file_replace_pattern(curves_list_path + "curves_list.h", magic_re, "#include <libecc/curves/user_defined/ec_params_"+name+".h>\n"+magic_back)
    # Add the curve mapping
    magic = "ADD curves mapping here"
    magic_re = "\/\* "+magic+" \*\/"
    magic_back = "/* "+magic+" */"
    file_replace_pattern(curves_list_path + "curves_list.h", magic_re, "#ifdef WITH_CURVE_"+name.upper()+"\n\t{ .type = "+name.upper()+", .params = &"+name+"_str_params },\n#endif /* WITH_CURVE_"+name.upper()+" */\n"+magic_back)
    # Add the new curve type in the enum
    # First we get the number of already defined curves so that we increment the enum counter
    num_with_curve = num_patterns_in_file(lib_ecc_types_path + "lib_ecc_types.h", "#ifdef WITH_CURVE_")
    magic = "ADD curves type here"
    magic_re = "\/\* "+magic+" \*\/"
    magic_back = "/* "+magic+" */"
    file_replace_pattern(lib_ecc_types_path + "lib_ecc_types.h", magic_re, "#ifdef WITH_CURVE_"+name.upper()+"\n\t"+name.upper()+" = "+str(num_with_curve+1)+",\n#endif /* WITH_CURVE_"+name.upper()+" */\n"+magic_back)
    # Add the new curve define in the config
    magic = "ADD curves define here"
    magic_re = "\/\* "+magic+" \*\/"
    magic_back = "/* "+magic+" */"
    file_replace_pattern(lib_ecc_config_path + "lib_ecc_config.h", magic_re, "#define WITH_CURVE_"+name.upper()+"\n"+magic_back)
    # Add the new curve meson option in the meson.options file
    magic = "ADD curves meson option here"
    magic_re = "# " + magic
    magic_back = "# " + magic
    file_replace_pattern(meson_options_path + "meson.options", magic_re, "\t'"+name.lower()+"',\n"+magic_back)

    # Do we need to add some test vectors?
    if add_test_vectors != None:
        print("Test vectors generation asked: this can take some time! Please wait ...")
        # Create curve
        c = Curve(a, b, prime, order, cofactor, gx, gy, cofactor * order, name, oid)
        # Generate key pair for the algorithm
        vectors = gen_self_tests(c, add_test_vectors)
        # Iterate through all the tests
        f = open(ec_self_tests_path + "ec_self_tests_core_"+name+".h", 'w')
        for l in vectors:
            for v in l:
                for case in v:
                    (case_name, case_vector) = case
                    # Add the new test case
                    magic = "ADD curve test case here"
                    magic_re = "\/\* "+magic+" \*\/"
                    magic_back = "/* "+magic+" */"
                    file_replace_pattern(ec_self_tests_path + "ec_self_tests_core.h", magic_re, case_name+"\n"+magic_back)
                    # Create/Increment the header file
                    f.write(case_vector)
        f.close()
        # Add the new test cases header
        magic = "ADD curve test vectors header here"
        magic_re = "\/\* "+magic+" \*\/"
        magic_back = "/* "+magic+" */"
        file_replace_pattern(ec_self_tests_path + "ec_self_tests_core.h", magic_re, "#include \"ec_self_tests_core_"+name+".h\"\n"+magic_back)
    return True


#### Main
if __name__ == "__main__":
    signal.signal(signal.SIGINT, handler)
    parse_cmd_line(sys.argv[1:])
